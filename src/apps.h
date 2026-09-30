#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.h"
#include "luna.h"

struct AppEntry {
    std::string id;
    std::string title;
    std::string icon;               // absolute path to a PNG, may be empty
    std::optional<uint32_t> color;  // tile background (0xRRGGBB) from iconColor/bgColor
    nlohmann::json params;          // launch params (set for bookmark launch points)
};

class AppSource {
public:
    AppSource(const Config& cfg, LunaClient& luna);

    // Queries the installed apps, drops blacklisted ones and this app itself.
    std::vector<AppEntry> list();
    bool launch(const AppEntry& app);

    // Why the last list() came back empty (one line per call tried).
    const std::string& lastError() const { return lastError_; }

private:
    const Config& cfg_;
    LunaClient& luna_;
    const char* mockFile_; // $WEBOS_HOME_MOCK: read the listing from a file (desktop testing)
    std::string lastError_;
};

// Parses a listLaunchPoints reply ({"launchPoints": [...]}) or a listApps
// reply ({"apps": [...]}). Exposed for testing.
std::vector<AppEntry> parseAppList(const std::string& json);
