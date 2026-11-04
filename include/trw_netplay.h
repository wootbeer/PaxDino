#ifndef __TRW_NETPLAY_H__
#define __TRW_NETPLAY_H__

#include <cstdint>
#include <string>
#include <vector>

#include "trw_input_record.h"

// Netplay (src/main/netplay.cpp): two players in lockstep by direct IP.
namespace trw::netplay {
    // Before the game starts (src/main/main.cpp): the game's version (both
    // players must have the same build), and PAXDINO_NETPLAY if it's set (the
    // joiner waits for the host's answer then).
    void start(const char* version);
    // The session has started: the game plays online.
    bool active();
    // This PC's player's controller port (0 hosting, 1 joining).
    int local_port();
    // A joiner whose host is gone (left or lost): it plays on alone.
    bool host_gone();
    // One sweep of the game's controller reads: hands over this player's
    // read and fills `out` with every port's for this sweep, waiting for the
    // other player's if it hasn't come.
    void exchange(const trw::inputrec::PortRead& local_read, trw::inputrec::PortRead out[4]);

    // The launcher's Netplay (src/main/online_menu.cpp), before the game
    // starts. The host: Waiting while players join (lobby_players), until
    // host_start (then begin and start the game). The joiner: Waiting for the
    // host's answer, Found with its settings (show them, then accept or
    // cancel), Waiting again for the host to start, Go once it has (begin,
    // then start the game). Failed: lobby_error_text says why.
    enum class Lobby { Idle, Waiting, Found, Failed, Go };
    bool host(uint16_t port, std::string& error);
    bool join(const std::string& address, uint16_t port, std::string& error);
    // Join codes (tools/matchmaker): the host gets one when it hosts, if the
    // build has the service's address; the joiner can type it instead of an
    // address.
    bool looks_like_code(const std::string& text);
    bool join_by_code(const std::string& code, std::string& error);
    std::string join_code();      // the host's, once got ("" until then, or none)
    // Public games (the service's list): the host's choice, before host();
    // a lobby name to list it under.
    void set_public(bool listed, const std::string& name);
    struct PublicGame {
        std::string code;
        int players; // in it now (1 to 3: full ones aren't listed)
        std::string name;
    };
    enum class ListState { Idle, Loading, Done, Failed };
    // Asks for the list (off the UI's thread); public_games gives the last
    // answer and whether one is on its way.
    void refresh_public_games();
    ListState public_games(std::vector<PublicGame>& games);
    std::string join_code_note(); // the host: while getting it, or why there's none
    Lobby poll_lobby(); // each launcher frame
    struct LobbyPlayer {
        int port;   // controller port, for now (they're numbered in order at the start)
        bool ready; // has accepted the settings
        std::string name; // their nickname ("" if none)
    };
    std::vector<LobbyPlayer> lobby_players(); // the host: who has joined
    bool host_start();                // the host: start with those who are ready (false if none)
    std::string lobby_error_text();
    // The host: a player on another version tried to join ("" if not).
    std::string version_note_text();
    std::string host_settings_text(); // the joiner, once Found
    void accept();                    // the joiner: Ready, then wait for the host's Start
    void cancel();                    // back to Idle (not once begun)
    void begin();                     // the session starts; then start the game
    // The game is waiting for the other player's input: the screen is shown
    // again as it was (src/main/main.cpp), with the message over it.
    bool screen_held();
    // Quitting: tells the other player, who plays on without this one.
    void shutdown();
    // The session's players, for the menu's Netplay tab.
    struct Player {
        int port;           // controller port (0 player 1)
        std::string name;   // player_name
        bool local;         // this PC's
        bool host;
        std::string where;  // the host's address (to a joiner who typed it)
        std::string status; // "Connected", "Gone", "Left the game"...
        int ping_ms;        // the round trip to them, if this PC talks to them (-1 if not)
    };
    // This PC's player's nickname, before host() or join(): letters, digits
    // and a little punctuation, no spaces, 12 characters at most (anything
    // else is dropped).
    void set_nickname(const std::string& name);
    // A port's player's nickname this session, or "Player N".
    std::string player_name(int port);
    std::vector<Player> players();
    // The session's players (How Many Players is set to it,
    // src/main/fps_controls.cpp).
    int players_playing();
    uint32_t input_delay(); // in controller reads
    // The host's input delay, in controller reads; 0 Auto (the joiner picks
    // it from the ping). Before hosting.
    void set_delay_choice(uint32_t sweeps);
    // The host's Unlock Everything (on unless they turn it off): the session
    // has every level, skin, character and weapon (the Unlocks tab's), whatever
    // the host's own tab says. Extra Modes (Time Trial, Frag Fest) is never on
    // online. Before hosting.
    void set_unlock_all(bool on);
    // Controller Paks in netplay (each player's own, and Load Profiles), held
    // off for now: off, a session has no paks, as before they came in.
    constexpr bool kControllerPaks = false;
    // The host's Load Profiles: each player's first profile on their
    // Controller Pak loaded at the session's start (src/main/fps_controls.cpp),
    // so matches count toward their stats. Before hosting.
    void set_load_profiles(bool on);
    uint32_t host_delay_choice(); // the joiner, once Found: the host's (0 Auto)
    int ping_ms();    // the usual round trip: the joiner's to the host, the host's slowest; -1 not known
    float sweep_ms(); // about how long a controller read lasts
    // Chat (src/main/chat.cpp): sends a message to every player, through
    // the host; false if there's nobody to send it to. They show it with
    // trw::chat::add_line.
    bool send_chat(const std::string& text);
    // This PC's address on its network (for the host to tell the joiner).
    std::string lan_address();
}

#endif
