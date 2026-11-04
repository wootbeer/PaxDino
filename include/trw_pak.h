#ifndef __TRW_PAK_H__
#define __TRW_PAK_H__

#include <array>
#include <cstdint>
#include <vector>

// Controller Paks in netplay (src/game/controller_pak_hle.cpp). Every PC
// plays with the same set of paks: each player's own, sent to the others in
// the lobby (src/main/netplay.cpp). They live in memory for the session;
// each PC writes back only its own player's file.
namespace trw::pak {
    constexpr size_t kImageSize = 32 * 1024;

    // This PC's own Controller Pak, player 1's file, as it is on disk: false
    // if player 1 has no Controller Pak (General tab) or no valid file.
    bool read_own(std::vector<uint8_t>& image);
    // Whether `image` is a pak the game can use: the size, and an inode
    // table (or its backup) whose checksum holds.
    bool valid_image(const std::vector<uint8_t>& image);

    // The session's paks, by controller port (empty: none there), before
    // the game starts; `own_port` is this PC's player's. Its file is copied
    // to a backup first (_pak1.netplay.bak).
    void begin_session(const std::array<std::vector<uint8_t>, 4>& images, int own_port);
    // A pak on `port` this session (get_accessory, src/main/game_config.cpp).
    bool session_has(int port);
    // That pak holds a Turok: Rage Wars player profile (a note of the game's).
    bool session_has_profile(int port);
}

#endif
