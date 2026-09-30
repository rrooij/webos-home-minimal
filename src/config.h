#pragma once

#include <string>
#include <vector>

enum class IconPosition {
    Center,
    Top,
    Bottom,
};

struct Config {
    std::string path;                  // file the config was loaded from (empty = defaults)
    std::vector<std::string> lunaSend; // luna-send binaries to try, in order
    std::vector<std::string> blacklist; // app id patterns (fnmatch-style, e.g. "com.webos.app.*")
    bool sortByTitle = false;
    bool useHbchannel = true; // fall back to the Homebrew Channel root service
    IconPosition iconPosition = IconPosition::Center;
    std::string background;   // "background.jpeg" if present in the working directory

    bool isBlacklisted(const std::string& appId) const;
};

// Looks for the config in, in order:
//   1. the path given as first command line argument
//   2. $WEBOS_HOME_CONFIG
//   3. $HOME/.config/webos-home-minimal/home.conf
//   4. home.conf next to the executable (i.e. inside the installed app folder)
// The background image is background.jpeg in the working directory.
Config loadConfig(int argc, char** argv);
