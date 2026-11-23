#ifndef __TRW_COMMAND_LINE_H__
#define __TRW_COMMAND_LINE_H__

#include <string>
#include <utility>
#include <vector>

// The command line (src/main/command_line.cpp):
//   PaxDino --boot             play at once, without the launcher
//   PaxDino --rom FILE         use FILE as the game's ROM (kept, as Load ROM)
//   PaxDino --netplay          open the Netplay screen
//   PaxDino --host             open the Netplay screen and host
//   PaxDino --join CODE        open the Netplay screen and join CODE (or an address)
//   PaxDino --NAME VALUE       any setting, for this run only (not saved)
//   PaxDino --help             list them all
namespace trw::cli {
    struct Options {
        bool help = false;
        bool boot = false;
        bool netplay = false;
        bool host = false;
        std::string join;
        std::string rom;
        std::vector<std::pair<std::string, std::string>> settings; // --NAME VALUE, in order
    };

    // First thing in main(): false, with `error` set, for an argument that
    // isn't one (settings' names are checked later, once they exist).
    bool parse(int argc, char** argv, std::string& error);
    const Options& options();

    // After the settings are loaded (recompui::config::finalize): sets the
    // command line's for this run. They're kept out of the files: saving a
    // tab writes the value the file had for each, unless it was changed in
    // the menu since. False, with `error` set, for an unknown name or value.
    bool apply_settings(std::string& error);

    // The --help text, with every setting (after the settings exist).
    std::string help_text(const std::string& title);

    // Text for the player: the log, the terminal it was started from if
    // there is one, and on Windows a message box when there isn't.
    void tell(const std::string& title, const std::string& text);
}

#endif
