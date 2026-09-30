#include "apps.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <strings.h>
#include <unistd.h>

using nlohmann::json;

namespace {

std::string str(const json& obj, const char* key) {
    auto it = obj.find(key);
    return it != obj.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<uint32_t> parseColor(const std::string& s) {
    if (s.size() != 7 || s[0] != '#')
        return std::nullopt;
    char* end = nullptr;
    unsigned long v = std::strtoul(s.c_str() + 1, &end, 16);
    if (*end != '\0')
        return std::nullopt;
    return static_cast<uint32_t>(v);
}

std::string resolveIcon(const json& obj) {
    std::string folder = str(obj, "folderPath");
    // Prefer the larger artwork; tiles are ~200px on a 1080p screen.
    for (const char* key : {"extraLargeIcon", "largeIcon", "mediumLargeIcon", "icon"}) {
        std::string p = str(obj, key);
        if (p.empty())
            continue;
        if (p.rfind("file://", 0) == 0)
            p.erase(0, 7);
        if (p[0] != '/' && !folder.empty())
            p = folder + "/" + p;
        if (access(p.c_str(), R_OK) == 0)
            return p;
    }
    return {};
}

// Current firmware (e.g. webOS 6) answers on com.webos.service.applicationmanager;
// the com.webos.applicationManager alias is kept as a fallback for older ones.
const char* const kServices[] = {
    "luna://com.webos.service.applicationmanager",
    "luna://com.webos.applicationManager",
};

} // namespace

std::vector<AppEntry> parseAppList(const std::string& text) {
    std::vector<AppEntry> apps;
    json reply = json::parse(text, nullptr, false);
    if (reply.is_discarded() || !reply.is_object()) {
        std::fprintf(stderr, "apps: invalid JSON reply\n");
        return apps;
    }
    if (reply.value("returnValue", true) == false) {
        std::fprintf(stderr, "apps: call failed: %s\n", reply.dump().c_str());
        return apps;
    }

    bool isLaunchPoints = reply.contains("launchPoints");
    const json& list = isLaunchPoints ? reply["launchPoints"] : reply.value("apps", json::array());
    if (!list.is_array())
        return apps;

    for (const auto& obj : list) {
        if (!obj.is_object())
            continue;
        // listApps also returns background services and hidden system apps.
        if (!isLaunchPoints) {
            auto cls = obj.find("class");
            bool classHidden = cls != obj.end() && cls->is_object() && cls->value("hidden", false);
            if (obj.value("visible", true) == false || obj.value("noWindow", false) || classHidden)
                continue;
        }

        AppEntry app;
        app.id = str(obj, "id");
        if (app.id.empty())
            continue;
        app.title = str(obj, "title");
        if (app.title.empty())
            app.title = app.id;
        app.icon = resolveIcon(obj);
        app.color = parseColor(str(obj, "iconColor"));
        if (!app.color)
            app.color = parseColor(str(obj, "bgColor"));
        if (auto it = obj.find("params"); it != obj.end() && it->is_object() && !it->empty())
            app.params = *it;
        apps.push_back(std::move(app));
    }
    return apps;
}

AppSource::AppSource(const Config& cfg, LunaClient& luna)
    : cfg_(cfg), luna_(luna), mockFile_(std::getenv("WEBOS_HOME_MOCK")) {}

std::vector<AppEntry> AppSource::list() {
    std::vector<AppEntry> apps;
    lastError_.clear();
    if (mockFile_) {
        std::ifstream in(mockFile_);
        std::stringstream ss;
        ss << in.rdbuf();
        apps = parseAppList(ss.str());
    } else {
        // Launch points are exactly what LG's own launcher shows (visible
        // apps, in the user's order); listApps is the fallback.
        for (const char* service : kServices) {
            for (const char* method : {"/listLaunchPoints", "/listApps"}) {
                std::string uri = std::string(service) + method;
                if (auto reply = luna_.call(uri, "{}")) {
                    apps = parseAppList(*reply);
                    if (apps.empty())
                        lastError_ += uri + ": no visible apps in reply\n";
                } else {
                    lastError_ += uri + ": " + luna_.lastError() + "\n";
                }
                if (!apps.empty()) {
                    std::fprintf(stderr, "apps: %zu from %s\n", apps.size(), uri.c_str());
                    break;
                }
            }
            if (!apps.empty())
                break;
        }
        if (!apps.empty())
            lastError_.clear();
    }

    apps.erase(std::remove_if(apps.begin(), apps.end(),
                              [&](const AppEntry& a) { return a.id == APP_ID || cfg_.isBlacklisted(a.id); }),
               apps.end());

    if (cfg_.sortByTitle)
        std::stable_sort(apps.begin(), apps.end(), [](const AppEntry& a, const AppEntry& b) {
            return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
        });
    return apps;
}

bool AppSource::launch(const AppEntry& app) {
    json payload = {{"id", app.id}};
    if (!app.params.is_null())
        payload["params"] = app.params;

    std::fprintf(stderr, "launch: %s\n", payload.dump().c_str());
    if (mockFile_)
        return true;
    for (const char* service : kServices) {
        // LunaClient already rejects replies with "returnValue": false.
        if (luna_.call(std::string(service) + "/launch", payload.dump()))
            return true;
    }
    return false;
}
