#ifndef __TRW_PORTABLE_H__
#define __TRW_PORTABLE_H__

#include <string>

// Portable by default (src/main/portable.cpp): a new player's settings,
// saves and ROM go in the game's own folder, marked by a portable.txt there.
namespace trw::portable {
    // Once, after the program id is set and before the config folder is
    // used. Makes portable.txt in the game's folder when there's nothing to
    // keep elsewhere: no settings in the user folder, none beside the game
    // without portable.txt (the player deleted it), and the folder can be
    // written. In an AppImage the game's folder is the one holding the
    // AppImage (its own is read-only). Players already set up either way
    // stay as they are.
    void choose();
    // Where the data is and why, for the log ("portable (portable.txt)").
    const std::string& note();
}

#endif
