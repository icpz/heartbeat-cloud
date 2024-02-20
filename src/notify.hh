#pragma once

#include <filesystem>
#include <string>
#include <future>

enum NotifyEvent {
    INFO,
    ERROR,
    NEWBIE,
    EXPIRE,
};

std::future<void> NotifyLaunch(const std::filesystem::path &prog, NotifyEvent event, const std::string &arg);

