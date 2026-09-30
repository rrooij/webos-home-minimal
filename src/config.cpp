#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <fnmatch.h>
#include <fstream>
#include <unistd.h>

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos)
        return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string exeDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return ".";
    std::string p(buf, static_cast<size_t>(n));
    auto slash = p.rfind('/');
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

bool parseBool(const std::string& v) {
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

void parseFile(std::ifstream& in, Config& cfg) {
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line.erase(hash);
        line = trim(line);
        if (line.empty())
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            std::fprintf(stderr, "%s:%d: expected key = value\n", cfg.path.c_str(), lineNo);
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "blacklist")
            cfg.blacklist.push_back(value);
        else if (key == "luna_send")
            cfg.lunaSend = {value};
        else if (key == "sort")
            cfg.sortByTitle = value == "title";
        else if (key == "sort_by_title")
            cfg.sortByTitle = parseBool(value);
        else if (key == "use_hbchannel")
            cfg.useHbchannel = parseBool(value);
        else if (key == "position" || key == "icon_position") {
            std::string v = value;
            if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\'')))
                v = v.substr(1, v.size() - 2);
            for (char& c : v)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "top")
                cfg.iconPosition = IconPosition::Top;
            else if (v == "bottom")
                cfg.iconPosition = IconPosition::Bottom;
            else if (v == "center" || v == "middle")
                cfg.iconPosition = IconPosition::Center;
            else
                std::fprintf(stderr, "%s:%d: unknown position '%s' (expected center, top, or bottom)\n",
                             cfg.path.c_str(), lineNo, value.c_str());
        } else
            std::fprintf(stderr, "%s:%d: unknown key '%s'\n", cfg.path.c_str(), lineNo, key.c_str());
    }
}

} // namespace

bool Config::isBlacklisted(const std::string& appId) const {
    for (const auto& pattern : blacklist)
        if (fnmatch(pattern.c_str(), appId.c_str(), 0) == 0)
            return true;
    return false;
}

Config loadConfig(int argc, char** argv) {
    std::vector<std::string> candidates;
    if (argc > 1)
        candidates.emplace_back(argv[1]);
    if (const char* env = std::getenv("WEBOS_HOME_CONFIG"))
        candidates.emplace_back(env);
    if (const char* home = std::getenv("HOME"))
        candidates.push_back(std::string(home) + "/.config/webos-home-minimal/home.conf");
    candidates.push_back(exeDir() + "/home.conf");

    Config cfg;
    // Absolute paths too, in case PATH is minimal in the app's environment.
    cfg.lunaSend = {"luna-send", "luna-send-pub", "/usr/bin/luna-send", "/usr/bin/luna-send-pub"};
    for (const auto& path : candidates) {
        std::ifstream in(path);
        if (!in)
            continue;
        cfg.path = path;
        parseFile(in, cfg);
        const char* posStr = "center";
        if (cfg.iconPosition == IconPosition::Top)
            posStr = "top";
        else if (cfg.iconPosition == IconPosition::Bottom)
            posStr = "bottom";
        std::fprintf(stderr, "config: %s (%zu blacklist entries, position: %s)\n",
                     path.c_str(), cfg.blacklist.size(), posStr);
        break;
    }

    // Relative to the working directory, which webOS sets to the app folder.
    if (access("background.jpeg", R_OK) == 0) {
        cfg.background = "background.jpeg";
        std::fprintf(stderr, "background: background.jpeg\n");
    }
    return cfg;
}
