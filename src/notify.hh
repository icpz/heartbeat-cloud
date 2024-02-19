#pragma once

#include <filesystem>
#include <string>

enum NotifyEvent {
    INFO,
    ERROR,
    NEWBIE,
    EXPIRE,
};

void NotifyLaunch(const std::filesystem::path &prog, NotifyEvent event, const std::string &arg);

