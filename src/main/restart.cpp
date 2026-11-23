// Restart: a button in the menu's header between Quit and Close, and a
// choice in the quit prompt (lib-patches/RecompFrontend/0020). It quits as
// Quit does, and just before the process ends (main's end, or the crash
// handler's quiet exit once quitting has started) starts the game again with
// the same command line and working folder. Handy after changing settings
// that take effect at the next start (Graphics API, Local Multiplayer).
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <spawn.h>
#include <unistd.h>
extern char** environ;
#endif

#include "recompui/recompui.h"
#include "ultramodern/ultramodern.hpp"

#include "trw_restart.h"

namespace {
    std::atomic_bool requested = false;
    std::atomic_bool relaunched = false;
#if !defined(_WIN32)
    std::vector<std::string> arguments;
#endif
}

void trw::restart::install(int argc, char** argv) {
#if !defined(_WIN32)
    arguments.assign(argv, argv + argc);
#else
    (void)argc;
    (void)argv;
#endif
    recompui::set_restart_callback([]() {
        requested = true;
        ultramodern::quit();
    });
}

void trw::restart::relaunch_if_requested() {
    if (!requested.load() || relaunched.exchange(true)) {
        return;
    }
    std::fprintf(stderr, "[TRW] restarting\n");
#if defined(_WIN32)
    wchar_t exe[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
    if (length == 0 || length >= std::size(exe)) {
        std::fprintf(stderr, "Restart: couldn't find the game's exe\n");
        return;
    }
    // CreateProcessW may write to the command line, so a copy.
    std::wstring command_line = GetCommandLineW();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        AllowSetForegroundWindow(process.dwProcessId); // this one has the focus: the new one may take it
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    else {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %lu)\n", GetLastError());
    }
#else
    // In an AppImage, the AppImage itself: this binary is in its mount,
    // which goes once this process has.
    // The file itself, not /proc/self/exe: started from that, the new
    // process would be named "exe" in the task list.
    std::string exe = "/proc/self/exe";
    char path[4096];
    if (ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1); length > 0) {
        path[length] = '\0';
        exe = path;
    }
    if (const char* appimage = std::getenv("APPIMAGE"); appimage != nullptr && appimage[0] != '\0') {
        exe = appimage;
    }
    std::vector<char*> argv;
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    pid_t pid;
    int error = posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv.data(), environ);
    if (error != 0) {
        std::fprintf(stderr, "Restart: couldn't start the game again (error %d)\n", error);
    }
#endif
}
