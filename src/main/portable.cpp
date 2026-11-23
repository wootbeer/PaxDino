// Portable by default (include/trw_portable.h). recompui keeps the settings,
// saves and ROM in the working folder when a portable.txt is there, else in
// the user folder (%LOCALAPPDATA%\trwrecomp, ~/.config/trwrecomp, or on Linux
// APP_FOLDER_PATH when set). A new player gets portable.txt made for them; a
// player already set up either way is left as they are, and can delete
// portable.txt (or add one) to change it.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "trw_portable.h"
#include "recompui/recompui.h"
#include "util/file.h"

namespace {
    std::string data_note = "the user folder";

    bool has_files(const std::filesystem::path& folder) {
        std::error_code ec;
        return std::filesystem::is_directory(folder, ec) && !std::filesystem::is_empty(folder, ec) && !ec;
    }
}

void trw::portable::choose() {
    namespace fs = std::filesystem;
    std::error_code ec;
    // The game's folder: the working folder (the game needs it there: it
    // loads assets/ from it), or in an AppImage, the AppImage's own folder.
    fs::path base;
    bool appimage = false;
#if defined(__linux__)
    if (const char* image = std::getenv("APPIMAGE"); image != nullptr && image[0] != '\0') {
        if (std::getenv("APP_FOLDER_PATH") != nullptr) {
            data_note = "APP_FOLDER_PATH";
            return; // chosen by the player
        }
        base = fs::path(image).parent_path();
        appimage = true;
    }
#endif
    if (!appimage) {
        base = fs::current_path(ec);
        if (ec || !fs::is_directory(base / "assets", ec)) {
            return; // not started from the game's folder: as before
        }
    }
    auto use_base = [&]() {
#if defined(__linux__)
        if (appimage) {
            setenv("APP_FOLDER_PATH", base.string().c_str(), 1);
        }
#endif
    };

    if (fs::exists(base / "portable.txt", ec)) {
        use_base();
        data_note = "portable (portable.txt)";
        return;
    }
    if (has_files(recompui::file::get_app_folder_path())) {
        data_note = "the user folder (already set up there)";
        return;
    }
    // Settings beside the game without portable.txt: it was deleted, so the
    // player wants the user folder.
    for (const char* settings : { "general.json", "graphics.json", "controls.json" }) {
        if (fs::exists(base / settings, ec)) {
            data_note = "the user folder (portable.txt was removed)";
            return;
        }
    }
    {
        std::ofstream file(base / "portable.txt");
        file << "PaxDino keeps its settings, saves and ROM in this folder.\n"
                "Delete this file to keep them in your user folder instead.\n";
        if (!file.good()) {
            file.close();
            fs::remove(base / "portable.txt", ec);
            data_note = "the user folder (the game's folder can't be written)";
            return;
        }
    }
    use_base();
    data_note = "portable (new: portable.txt made)";
}

const std::string& trw::portable::note() {
    return data_note;
}
