#pragma once

#include <optional>
#include <string>
#include <vector>

// Minimal Luna bus client. It shells out to `luna-send` instead of linking
// libluna-service2: no service registration or role files are needed, and the
// binary can be swapped for `luna-send-pub` from the config file.
class LunaClient {
public:
    // Candidates are tried in order until one succeeds; the first one that
    // works is remembered.
    // useHbchannel: when direct calls fail, retry them as root through the
    // Homebrew Channel service (rooted TVs only).
    LunaClient(std::vector<std::string> candidates, bool useHbchannel);

    // Performs a single call and returns the raw JSON reply. A reply with
    // "returnValue": false (e.g. access denied) counts as a failure, so the
    // next binary gets a chance; on failure lastError() says what went wrong.
    std::optional<std::string> call(const std::string& uri, const std::string& payload);

    const std::string& lastError() const { return lastError_; }

private:
    std::optional<std::string> callDirect(const std::string& uri, const std::string& payload, std::string& errors);
    std::optional<std::string> callViaHbchannel(const std::string& uri, const std::string& payload,
                                                std::string& errors);

    std::vector<std::string> candidates_;
    bool useHbchannel_;
    bool viaHbchannel_ = false; // direct calls were denied, the proxy worked
    std::string lastError_;
};

// Runs argv[0] with the given arguments and returns its stdout, or nullopt if
// it could not be started, timed out or exited non-zero (described in *error).
std::optional<std::string> runCapture(const std::vector<std::string>& argv, int timeoutMs,
                                      std::string* error = nullptr);
