#ifndef __TRW_ONLINE_MENU_H__
#define __TRW_ONLINE_MENU_H__

#include <string>

// The launcher's Netplay (src/main/online_menu.cpp).
namespace trw::online_menu {
    // `game_id`: the game to start once the players are connected.
    void open(const std::u8string& game_id);
    // Each launcher frame.
    void update();

    // A message over the game (src/main/netplay.cpp): for `seconds`, or
    // until replaced if 0; an empty one hides it.
    void show_message(const std::string& text, int seconds);
    // Hides the message once its time is up (each controller sweep).
    void message_tick();

    // The menu's Netplay tab (F2 during a session), hidden until then.
    void create_netplay_tab();
    // The session has begun: mods off, the tabs that change the game hidden,
    // the Netplay tab shown (on the UI thread).
    void use_session_menu();
    // The Netplay tab's Leave Game: the game quits, and main() starts it
    // again on the Netplay screen (--netplay).
    bool leaving_to_netplay_screen();
}

#endif
