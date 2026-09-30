#include "luna.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

std::optional<std::string> runCapture(const std::vector<std::string>& argv, int timeoutMs,
                                      std::string* error) {
    auto fail = [&](std::string why) -> std::optional<std::string> {
        if (error)
            *error = std::move(why);
        return std::nullopt;
    };

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0)
        return fail("pipe failed");

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return fail("fork failed");
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
            dup2(devnull, STDERR_FILENO);
        std::vector<char*> args;
        for (const auto& a : argv)
            args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(fds[1]);

    std::string out;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    bool timedOut = false;
    for (;;) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            timedOut = true;
            break;
        }
        pollfd pfd{fds[0], POLLIN, 0};
        int r = poll(&pfd, 1, static_cast<int>(left));
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0) {
            timedOut = r == 0;
            break;
        }
        char buf[4096];
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        out.append(buf, static_cast<size_t>(n));
    }
    close(fds[0]);

    if (timedOut)
        kill(pid, SIGKILL);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (timedOut)
        return fail("timed out");
    if (!WIFEXITED(status))
        return fail("killed by signal " + std::to_string(WTERMSIG(status)));
    if (WEXITSTATUS(status) == 127)
        return fail("not found");
    if (WEXITSTATUS(status) != 0)
        return fail("exit status " + std::to_string(WEXITSTATUS(status)) + (out.empty() ? "" : ": " + out));
    return out;
}

namespace {

// Validates a luna-send reply: must be a JSON object without "returnValue": false.
bool checkReply(const std::string& out, std::string& why) {
    auto reply = nlohmann::json::parse(out, nullptr, false);
    if (reply.is_discarded() || !reply.is_object()) {
        why = "invalid reply";
        return false;
    }
    if (reply.value("returnValue", true) == false) {
        why = reply.value("errorText", std::string("returnValue false"));
        return false;
    }
    return true;
}

std::string shellQuote(const std::string& s) {
    std::string q = "'";
    for (char c : s) {
        if (c == '\'')
            q += "'\\''";
        else
            q += c;
    }
    return q + "'";
}

void appendError(std::string& errors, const std::string& what) {
    if (!errors.empty())
        errors += "; ";
    errors += what;
}

} // namespace

LunaClient::LunaClient(std::vector<std::string> candidates, bool useHbchannel)
    : candidates_(std::move(candidates)), useHbchannel_(useHbchannel) {}

std::optional<std::string> LunaClient::call(const std::string& uri, const std::string& payload) {
    std::string errors;
    // Once the root proxy was needed, go there first instead of re-trying
    // calls we already know are denied.
    if (viaHbchannel_)
        if (auto out = callViaHbchannel(uri, payload, errors))
            return out;
    if (auto out = callDirect(uri, payload, errors))
        return out;
    if (useHbchannel_ && !viaHbchannel_) {
        if (auto out = callViaHbchannel(uri, payload, errors)) {
            viaHbchannel_ = true;
            return out;
        }
    }
    lastError_ = errors;
    std::fprintf(stderr, "luna call %s failed: %s\n", uri.c_str(), errors.c_str());
    return std::nullopt;
}

std::optional<std::string> LunaClient::callDirect(const std::string& uri, const std::string& payload,
                                                  std::string& errors) {
    for (size_t i = 0; i < candidates_.size(); ++i) {
        std::string why;
        auto out = runCapture({candidates_[i], "-n", "1", uri, payload}, 5000, &why);
        if (out && checkReply(*out, why)) {
            if (i != 0) // remember the binary that works
                std::swap(candidates_[0], candidates_[i]);
            lastError_.clear();
            return out;
        }
        appendError(errors, candidates_[i] + ": " + why);
    }
    return std::nullopt;
}

// Rooted TVs: the Homebrew Channel service runs as root and executes shell
// commands for any caller, so it can make the private-bus call on our behalf.
std::optional<std::string> LunaClient::callViaHbchannel(const std::string& uri, const std::string& payload,
                                                        std::string& errors) {
    nlohmann::json exec = {{"command", "luna-send -n 1 " + shellQuote(uri) + " " + shellQuote(payload)}};
    std::string execPayload = exec.dump();
    for (const auto& bin : candidates_) {
        std::string why;
        auto out = runCapture({bin, "-n", "1", "luna://org.webosbrew.hbchannel.service/exec", execPayload}, 10000,
                              &why);
        if (out && checkReply(*out, why)) {
            auto reply = nlohmann::json::parse(*out);
            std::string inner = reply.value("stdoutString", std::string());
            if (checkReply(inner, why)) {
                lastError_.clear();
                return inner;
            }
            std::string err = reply.value("stderrString", std::string());
            if (!err.empty())
                why += " (" + err + ")";
        }
        appendError(errors, "hbchannel exec via " + bin + ": " + why);
    }
    return std::nullopt;
}
