#ifndef __TRW_CHAT_H__
#define __TRW_CHAT_H__

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

union SDL_Event;

// Netplay chat (src/main/chat.cpp): a few lines at the left of the screen,
// and a line to type in, opened by the Chat binding (Enter or T on the
// keyboard by default; T only in the game's menus) during a match or in the
// menus. Enter sends;
// Escape clears what's typed, then closes it. Messages go through the host (src/main/netplay.cpp).
namespace trw::chat {
    constexpr size_t kMaxLength = 255;

    // Each SDL event, before the game's own key handling
    // (src/main/fps_controls.cpp): true if the chat took it.
    bool handle_event(const SDL_Event& event);
    // Each controller sweep (src/main/fps_controls.cpp): where the game is,
    // for where the chat can open. In the menus Enter is the game's
    // (select), so only the other Chat keys (T) open it there.
    enum class Where { Nowhere = 0, Menus = 1, Match = 2 };
    void set_allowed(Where where);
    // The chat line is open: this PC's player gets no input.
    bool typing();
    // N64 buttons to take out of this PC's read: those bound to a Chat key or
    // button that's held, so opening the chat doesn't also press them.
    uint16_t chat_held_buttons();
    // Default bindings, with the others (src/main/fps_controls.cpp).
    void set_defaults();
    // Once: saved profiles still on the first defaults get the new ones.
    // After the controls config is loaded.
    void update_profiles(const std::filesystem::path& config_dir);

    // A player's message, from netplay (any thread). `port` 0 is player 1.
    void add_line(int port, std::string_view text);
    // Each controller sweep (src/main/netplay.cpp): the box as it is now.
    void tick();

    // Printable ASCII only, no line breaks, at most kMaxLength, trimmed.
    std::string sanitize(std::string_view text);
}

#endif
