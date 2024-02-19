
#include <sys/wait.h>
#include <spawn.h>
#include <thread>

#include <spdlog/spdlog.h>

#include "notify.hh"

static const char *event_to_string(NotifyEvent event) {
    const char *result = "";
    switch (event) {
    case INFO:
        result = "info";
        break;
    case ERROR:
        result = "error";
        break;
    case NEWBIE:
        result = "newbie";
        break;
    case EXPIRE:
        result = "expire";
        break;
    }
    return result;
}

void NotifyLaunch(const std::filesystem::path &prog, NotifyEvent event, const std::string &arg) {
    if (!std::filesystem::exists(prog)) {
        SPDLOG_WARN("Invalid program path {}", prog.string());
        return;
    }

    std::thread{[=]() {
        auto event_str = event_to_string(event);
        int ret = 0;
        pid_t child_pid;
        char *argv[] = {
            const_cast<char *>(prog.c_str()),
            const_cast<char *>(event_str),
            const_cast<char *>(arg.c_str()),
            nullptr,
        };

        SPDLOG_INFO("Launching child process: {} {} {}", prog.string(), event_str, arg);
        ret = posix_spawn(&child_pid, prog.c_str(), nullptr, nullptr, argv, nullptr);
        if (ret != 0) {
            SPDLOG_ERROR("Failed to spawn child process: {}", ret);
            return;
        }
        SPDLOG_INFO("Child process launched as {}", child_pid);

        int state;
        ret = waitpid(child_pid, &state, 0);
        if (ret == -1) {
            SPDLOG_ERROR("Failed to wait for child process {}: {}", child_pid, errno);
            return;
        }
        SPDLOG_INFO("Child process {} exit? {} with code? {}", child_pid, (bool)WIFEXITED(state), WEXITSTATUS(state));
    }}.detach();
}
