// Mouse and keyboard controls (include/trw_controls.h).
//
// Turok moves with the C buttons (forward, back, strafe) and turns and looks
// with the stick, so WASD goes to the C buttons. The mouse turns and looks by
// writing the player's angles directly, once per controller read, so aiming
// follows the mouse 1:1 instead of going through the stick's speed cap and
// response curve. Controllers keep using the stick as on the N64.
//
// Mouse buttons and the wheel can't be bound in RecompFrontend's controls
// menu (its mouse input is unimplemented), so they're fixed here. In a match:
// left click fires (Z), right click is L, the wheel steps through weapons
// (see WeaponPick, with Q, E and 1-8; R is the weapon's second function).
// In the front-end menus: the arrow keys, WASD and the wheel navigate, left
// click selects (A), right click goes back (B); R is C-up. With Mouse in
// Menus on (General), moving the mouse there shows a cursor that points at
// items (include/trw_menu_cursor.h); controllers work as before.
//
// Escape and Enter work by screen (see get_input): Escape opens the pause
// menu in a match and leaves it again, and goes back a step in the menus;
// Enter and E select in the menus. The overlay
// menu is on F1 and F10 (lib-patches/RecompFrontend), which also close it.
#include "trw_controls.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include "recompinput/input_mapping.h"
#include "recompinput/input_state.h"
#include "recompinput/profiles.h"
#include "recompinput/players.h"
#include "recompui/config.h"
#include "recompui/recompui.h"
#include "trw_config.h"
#include "trw_menu_cursor.h"

uint8_t* trw::controls::rdram = nullptr;

extern "C" int trw_on_title_screen(void);
extern "C" int trw_match_hud_drawn(void);
extern "C" int trw_player_live(uint8_t* rdram, uint32_t player); // src/game/extras.cpp
extern "C" int trw_pause_resume_direction(void);

namespace {
    using recompinput::GameInput;
    using recompinput::InputField;

    // N64 controller button bits.
    constexpr uint16_t BTN_A = 0x8000;
    constexpr uint16_t BTN_B = 0x4000;
    constexpr uint16_t BTN_Z = 0x2000;
    constexpr uint16_t BTN_L = 0x0020;
    constexpr uint16_t BTN_START = 0x1000;
    constexpr uint16_t BTN_DPAD_UP = 0x0800;
    constexpr uint16_t BTN_DPAD_DOWN = 0x0400;
    constexpr uint16_t BTN_DPAD_LEFT = 0x0200;
    constexpr uint16_t BTN_DPAD_RIGHT = 0x0100;

    // Radians per pixel of mouse motion: 0.003335 at 50% sensitivity (the
    // default), half that at 0% and three times it at 100%, evenly on each
    // side (each step the same ratio), so the whole slider is usable. 0%
    // itself is off (the cursor is freed).
    constexpr float mouse_radians_at_default = 0.003335f;
    constexpr float mouse_sensitivity_low = 2.0f;  // the default over 0%'s
    constexpr float mouse_sensitivity_high = 3.0f; // 100%'s over the default

    // RecompFrontend scales the motion by the sensitivity (percent / 100);
    // the look's own scale is applied instead.
    float mouse_look_radians(float scaled_pixels) {
        float percent = (float)recompui::config::general::get_mouse_sensitivity();
        if (percent <= 0.0f) {
            return 0.0f;
        }
        float pixels = scaled_pixels / (percent / 100.0f);
        float range = percent < 50.0f ? mouse_sensitivity_low : mouse_sensitivity_high;
        return pixels * mouse_radians_at_default * std::pow(range, (percent - 50.0f) / 50.0f);
    }

    // The player objects (the first one's pointer at 0x8013EF84, the rest
    // 0x16E8 after it, each with its controller port at +0x5D4; a list
    // through +0x16E0 holds only the first in a match) and
    // their angles, in radians: yaw in the character object at +0x6C, pitch
    // in the head rotation offset at +0x724. Addresses in
    // docs/RAM_NOTES.md.
    constexpr uint32_t player_pointer_addr = 0x8013EF84;
    constexpr uint32_t player_next_offset = 0x16E0;
    constexpr uint32_t player_port_offset = 0x5D4;
    constexpr uint32_t player_yaw_offset = 0x6C;
    constexpr uint32_t player_pitch_offset = 0x724;
    constexpr float pitch_limit = 1.5707960f; // just under 90 degrees
    constexpr float tau = 6.2831853f;

    // Mouse motion is read once per poll; the game can read the controllers
    // more than once per poll, so the angles are only moved once.
    std::atomic<bool> fresh_poll{ false };

    uint32_t* guest_word(uint32_t addr) {
        return reinterpret_cast<uint32_t*>(trw::controls::rdram + (addr - 0x80000000u));
    }
    float read_float(uint32_t addr) {
        float f;
        uint32_t bits = *guest_word(addr);
        memcpy(&f, &bits, sizeof(f));
        return f;
    }
    void write_float(uint32_t addr, float f) {
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        *guest_word(addr) = bits;
    }
    bool valid_object(uint32_t addr) {
        return addr >= 0x80000400u && addr < 0x80800000u - 0x800u && (addr & 3) == 0;
    }

    // The player object on controller port `port`, or 0. The players (and
    // bots) are also in a row, 0x16E8 apart, port n the n-th: the list
    // through +0x16E0 needn't reach the second player.
    uint32_t player_slot_on_port(int port) {
        uint32_t first = *guest_word(player_pointer_addr);
        uint32_t slot = first + (uint32_t)port * 0x16E8u;
        if (port >= 0 && port < 4 && valid_object(first) && valid_object(slot) &&
            (int32_t)*guest_word(slot + player_port_offset) == port && *guest_word(slot + 0x5D0) != 0) {
            return slot;
        }
        uint32_t player = first;
        for (int i = 0; i < 4 && valid_object(player); i++) {
            if ((int32_t)*guest_word(player + player_port_offset) == port) {
                return player;
            }
            player = *guest_word(player + player_next_offset);
        }
        return 0;
    }

    // Only a player in the match now: the first player's pointer is left as
    // it was when a match is quit (trw_player_live).
    uint32_t player_on_port(int port) {
        uint32_t player = player_slot_on_port(port);
        return player != 0 && trw_player_live(trw::controls::rdram, player) ? player : 0;
    }

    // The weapon wheel (player + 0x938, through its pending choice at
    // +0x37C) is held as it was when the match was paused. The game keeps
    // reading A as "next weapon" under the pause menu, whose items A (or
    // Enter) selects, so the wheel came up behind it and the weapon changed.
    constexpr uint32_t wheel_offset = 0x938;
    constexpr uint32_t wheel_size = 0x380;
    uint32_t held_wheel[4][wheel_size / 4];
    uint32_t held_wheel_player[4] = {};
    bool a_down[4] = {}; // A held at the last controller read, per port

    // A held when the pause menu closes (it selected Resume) is ignored until
    // released, so it doesn't go on to cycle the weapon.
    bool was_paused[4] = {};
    bool hold_off_a[4] = {};

    void release_a_after_pause(int port, uint16_t* buttons) {
        bool paused = *guest_word(0x801407D4) != 0;
        if (was_paused[port] && !paused) {
            hold_off_a[port] = true;
        }
        was_paused[port] = paused;
        if (hold_off_a[port]) {
            if (*buttons & BTN_A) {
                *buttons &= ~BTN_A;
            }
            else {
                hold_off_a[port] = false;
            }
        }
    }

    // A match in play (its HUD drawn): the player pointer is only good then.
    // While a level loads it can still point at the last level's player,
    // whose memory is being reused; writing the view angles or the wheel
    // there corrupted the loading level (a rare hang right before the
    // fly-through, more likely with the mouse or keys in use).
    bool match_live() {
        uint32_t mode = *guest_word(0x801407B8);
        return !(mode <= 4 || mode == 8 || mode == 20) && trw_match_hud_drawn();
    }

    void hold_wheels_while_paused() {
        if (!match_live()) {
            for (int port = 0; port < 4; port++) {
                held_wheel_player[port] = 0;
            }
            return;
        }
        bool paused = *guest_word(0x801407D4) != 0;
        for (int port = 0; port < 4; port++) {
            // Held on past the pause while the A that closed it is down: the
            // game resumes and reads it as "next weapon" in the same frame.
            bool hold = paused || (held_wheel_player[port] != 0 && a_down[port]);
            uint32_t player = hold ? player_on_port(port) : 0;
            if (player == 0) {
                held_wheel_player[port] = 0;
                continue;
            }
            if (held_wheel_player[port] != player) {
                for (uint32_t i = 0; i < wheel_size / 4; i++) {
                    held_wheel[port][i] = *guest_word(player + wheel_offset + i * 4);
                }
                held_wheel_player[port] = player;
            }
            else {
                for (uint32_t i = 0; i < wheel_size / 4; i++) {
                    *guest_word(player + wheel_offset + i * 4) = held_wheel[port][i];
                }
            }
        }
    }

    // The controller port the mouse and keyboard play on: port 1 in single
    // player, otherwise the player the keyboard is assigned to (Controls tab,
    // Assign players); -1 if none.
    int mouse_port() {
        if (recompinput::players::is_single_player_mode()) {
            return 0;
        }
        for (int i = 0; i < (int)recompinput::players::get_max_number_of_players(); i++) {
            if (recompinput::players::get_player_is_assigned(i) && recompinput::players::get_player(i).keyboard_enabled) {
                return i;
            }
        }
        return -1;
    }

    // Adds mouse motion (`dx`, `dy`, scaled by the sensitivity option) to
    // the yaw and pitch of `player`.
    void turn_player_by(uint32_t player, float dx, float dy) {
        if (trw::config::get_invert_mouse_y()) {
            dy = -dy;
        }
        float yaw = read_float(player + player_yaw_offset) + mouse_look_radians(dx);
        if (yaw > tau) yaw -= tau;
        if (yaw < -tau) yaw += tau;
        write_float(player + player_yaw_offset, yaw);

        // Screen y grows downward; pitch grows looking up.
        float pitch = read_float(player + player_pitch_offset) - mouse_look_radians(dy);
        pitch = std::clamp(pitch, -pitch_limit, pitch_limit);
        write_float(player + player_pitch_offset, pitch);
    }

    // Mouse look is applied once a game frame, as the camera is built (at
    // the start of func_00234FCC, trw::controls::camera_begin), with all the
    // mouse motion seen since (on the event thread, as it comes). The
    // controller is read on its own thread, at retraces not in step with the
    // game's frames, so motion taken at the reads gave frames uneven turns
    // (two reads' worth, or none), a stutter a controller's stick (applied by
    // the game in its frame) doesn't have. The reads only say whether plain
    // mouse look is on, and for which player.
    std::mutex turn_mutex;
    bool turn_kept = false;           // the last read of the mouse's port was plain mouse look (game thread)
    uint32_t look_player = 0;         // the mouse's player while plain mouse look is on
    float turn_pending_x = 0.0f, turn_pending_y = 0.0f; // mouse pixels not yet applied
    // The display list buffers the game builds frames in, by turns.
    constexpr uint32_t dl_head_addr = 0x8010A574;
    constexpr uint32_t dl_buffer_a = 0x806D2000;
    constexpr uint32_t dl_buffer_b = 0x806E9000;
    constexpr uint32_t dl_buffer_size = 0x17000;

    // 0 or 1 for an address in one of the buffers, -1 otherwise.
    int dl_buffer_index(uint32_t addr) {
        addr = (addr & 0x1FFFFFFFu) | 0x80000000u;
        if (addr >= dl_buffer_a && addr < dl_buffer_a + dl_buffer_size) return 0;
        if (addr >= dl_buffer_b && addr < dl_buffer_b + dl_buffer_size) return 1;
        return -1;
    }

    // The event thread: mouse motion not yet applied to the player. (The
    // same events RecompFrontend adds up for the game, with the same
    // exceptions.)
    void add_turn_motion(const SDL_MouseMotionEvent& motion) {
        std::scoped_lock lock(turn_mutex);
        if (look_player == 0 || !SDL_GetRelativeMouseMode() || recompinput::game_input_disabled()) {
            return;
        }
        turn_pending_x += (float)motion.xrel;
        turn_pending_y += (float)motion.yrel;
    }

    // The game's controller read of the mouse's port: plain mouse look on
    // (`player`) or off (0).
    void read_mouse_look(uint32_t player) {
        std::scoped_lock lock(turn_mutex);
        if (player != look_player) {
            // Starting or stopping: nothing pending.
            turn_pending_x = turn_pending_y = 0.0f;
        }
        look_player = player;
    }

    // Plain mouse look for the player on `port` (`look`: a fresh poll).
    void mouse_look(int port, uint32_t player, bool look) {
        if (look) {
            read_mouse_look(match_live() ? player : 0);
        }
        turn_kept = true;
    }

    // The player the mouse aims (its look in use), or 0.
    std::atomic<uint32_t> mouse_aims_player{ 0 };
    std::atomic<int> mouse_aims_port{ 0 };

    std::atomic<uint32_t> mouse_buttons{ 0 };
    // Buttons pressed since the last controller read, so a click shorter
    // than a frame still registers.
    std::atomic<uint32_t> mouse_pressed{ 0 };
    std::atomic<int32_t> wheel_steps{ 0 };
    // Escape and Enter presses (not held keys: one closing the overlay menu,
    // or still down when it closes, mustn't reach the game).
    std::atomic<int32_t> escape_presses{ 0 };
    std::atomic<int32_t> enter_presses{ 0 };
    std::atomic<int32_t> space_presses{ 0 };
    std::atomic<int32_t> q_presses{ 0 };
    std::atomic<int32_t> slot_request{ -1 }; // 1-8 pressed: the wheel slot (0 up, clockwise)

    // The cursor in the game's menus (include/trw_menu_cursor.h), in window
    // pixels. It moves only in the menus (the match's mouse look is apart:
    // it reads the mouse's motion itself, and only in play), and shows once
    // the mouse has moved a little there; keys or a controller hide it.
    std::atomic<bool> menu_mode{ false };
    std::atomic<bool> cursor_shown{ false };
    std::atomic<bool> cursor_moved{ false };
    std::atomic<float> cursor_x{ 0.0f }, cursor_y{ 0.0f };
    std::atomic<int> window_w{ 0 }, window_h{ 0 };
    float cursor_wake = 0.0f; // motion while hidden (the event thread)
    constexpr float kCursorWakeDistance = 6.0f;

    void move_cursor(const SDL_MouseMotionEvent& motion) {
        SDL_Window* window = SDL_GetWindowFromID(motion.windowID);
        int w = 0, h = 0;
        if (window != nullptr) {
            SDL_GetWindowSize(window, &w, &h);
        }
        if (w <= 0 || h <= 0) {
            return;
        }
        if (window_w.load() != w || window_h.load() != h) {
            window_w.store(w);
            window_h.store(h);
        }
        float x, y;
        if (SDL_GetRelativeMouseMode()) {
            x = cursor_x.load() + motion.xrel;
            y = cursor_y.load() + motion.yrel;
        }
        else {
            x = (float)motion.x;
            y = (float)motion.y;
        }
        cursor_x.store(std::clamp(x, 0.0f, (float)(w - 1)));
        cursor_y.store(std::clamp(y, 0.0f, (float)(h - 1)));
        if (!cursor_shown.load()) {
            cursor_wake += std::fabs((float)motion.xrel) + std::fabs((float)motion.yrel);
            if (cursor_wake < kCursorWakeDistance) {
                return;
            }
            cursor_shown.store(true);
        }
        cursor_wake = 0.0f;
        cursor_moved.store(true);
    }

    int SDLCALL watch_events(void*, SDL_Event* event) {
        switch (event->type) {
        case SDL_MOUSEMOTION:
            if (menu_mode.load()) {
                move_cursor(event->motion);
            }
            else {
                add_turn_motion(event->motion);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            mouse_buttons.fetch_or(SDL_BUTTON(event->button.button));
            mouse_pressed.fetch_or(SDL_BUTTON(event->button.button));
            break;
        case SDL_MOUSEBUTTONUP:
            mouse_buttons.fetch_and(~SDL_BUTTON(event->button.button));
            break;
        case SDL_KEYDOWN:
            if (event->key.repeat == 0 && !recompinput::game_input_disabled()) {
                SDL_Scancode sc = event->key.keysym.scancode;
                if (sc == SDL_SCANCODE_ESCAPE) {
                    escape_presses.fetch_add(1);
                }
                else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
                    enter_presses.fetch_add(1);
                }
                else if (sc == SDL_SCANCODE_SPACE) {
                    space_presses.fetch_add(1);
                }
                else if (sc == SDL_SCANCODE_Q) {
                    q_presses.fetch_add(1);
                }
                else if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_8) {
                    slot_request.store(sc - SDL_SCANCODE_1);
                }
            }
            break;
        case SDL_MOUSEWHEEL: {
            int32_t y = event->wheel.y;
            if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                y = -y;
            }
            wheel_steps.fetch_add(y);
            break;
        }
        default:
            break;
        }
        return 0;
    }

    // One wheel step becomes a short press (two controller reads) of
    // whatever the step maps to, with a gap before the next so the game sees
    // each press. step() returns +1 (wheel up), -1 (down) or 0.
    struct WheelPulse {
        int hold = 0;
        int gap = 0;
        int dir = 0;

        int step() {
            if (hold > 0) {
                hold--;
                if (hold == 0) {
                    gap = 2;
                }
                return dir;
            }
            if (gap > 0) {
                gap--;
                return 0;
            }
            int32_t steps = wheel_steps.load();
            if (steps != 0) {
                dir = steps > 0 ? 1 : -1;
                wheel_steps.fetch_sub(dir);
                hold = 2;
                return dir;
            }
            return 0;
        }
    } wheel_pulse;

    // A key press becomes a short press (four controller reads, so the game
    // sees it on at least one of its frames) of a button, with a gap before
    // the next.
    struct KeyPulse {
        std::atomic<int32_t>& presses;
        int hold = 0;
        int gap = 0;

        int start_screen = -1;

        // `screen`: where the press is going. A press ends early when that
        // changes (Enter selecting Resume mustn't go on as Start in the match).
        int held_reads() const {
            return hold;
        }

        bool step(int screen) {
            if (hold > 0 && screen != start_screen) {
                hold = 0;
                gap = 3;
                return false;
            }
            if (hold > 0) {
                hold--;
                if (hold == 0) {
                    gap = 3;
                }
                return true;
            }
            if (gap > 0) {
                gap--;
                return false;
            }
            if (presses.load() > 0) {
                presses.fetch_sub(1);
                hold = 3;
                start_screen = screen;
                return true;
            }
            return false;
        }
    };
    // Escape on the options opened from the pause list: they leave only
    // through their Resume Game item, so the cursor is moved there (the
    // stick, a short push at a time) and A pressed. Where it is comes from
    // the menu hook (trw_pause_resume_direction, src/game/widescreen.cpp).
    struct ResumeNav {
        bool active = false;
        int hold = 0;     // controller reads left on the current push
        int gap = 0;      // reads left before looking again
        int pushes = 0;
        bool select = false;
        float stick_y = 0.0f;

        void start() {
            *this = ResumeNav{};
            active = true;
        }

        // Adds this read's input; ends when the screen changes.
        void step(bool on_options, uint16_t* buttons, float* y) {
            if (!active) {
                return;
            }
            if (!on_options) {
                active = false;
                return;
            }
            if (hold > 0) {
                hold--;
                if (select) {
                    *buttons |= BTN_A;
                }
                else {
                    *y = stick_y;
                }
                return;
            }
            if (gap > 0) {
                gap--;
                return;
            }
            int dir = trw_pause_resume_direction();
            if (dir == 0) {
                select = true;
                hold = 3;
                gap = 30; // done once the options close
            }
            else if ((dir == 1 || dir == -1) && pushes < 16) {
                // Below Resume Game: up; above it: down.
                stick_y = dir == 1 ? 1.0f : -1.0f;
                pushes++;
                hold = 2;
                gap = 4;
            }
            else {
                active = false;
            }
        }
    } resume_nav;

    KeyPulse escape_pulse{ escape_presses };
    KeyPulse enter_pulse{ enter_presses };
    KeyPulse title_pulse{ space_presses }; // Space (and a click) past the title

    // Weapons by keyboard and mouse, through the game's own weapon wheel.
    // The wheel (held A) has 8 slots, picked by the stick's direction
    // (func_00217B3C: 0 up, then clockwise, with a dead zone); the weapon
    // switches as soon as it points at one. Opening it fills its slots
    // (func_002181FC, at player + 0x938 + slot * 0x18: the weapon at +0x1C,
    // -1 for none, usable at +0x24) and marks the current weapon's slot
    // (+0x37C). While it's open (E held) the mouse moves a virtual stick
    // instead of the view. Q and the mouse wheel step to the next or previous
    // usable slot, and 1-8 pick a slot, by opening the wheel for a moment,
    // pointing at the slot, and letting go.
    float wheel_cursor_x = 0.0f, wheel_cursor_y = 0.0f;
    // The player whose wheel is open for such a pick: not drawn then
    // (trw_wheel_hidden), until the wheel has closed again.
    std::atomic<uint32_t> picking_player{ 0 };
    struct WeaponPick {
        int phase = 0;  // 0 idle, 1 opening the wheel, 2 pointing, 3 letting go
        int reads = 0;  // controller reads in this phase
        int step = 0;   // +1 next / -1 previous slot, 0 for a given slot
        int slot = -1;
    } pick;
    constexpr uint32_t kWheelSlotSize = 0x18;

    // The game's top-level mode (the mode table at 0x800CD69C, current mode
    // at 0x801407B8). 0-4 are the boot screens and title, 8 the front end
    // (main menu, options, character and weapon select), 20 the fade between
    // modes; everything else is a match. A paused match (0x801407D4 set)
    // takes the menus' mouse controls too.
    // The boot screens skip on a button press (func_002937F0 moves on when
    // buttons were pressed), except that the expansion pak screen (mode 1,
    // func_00293E30) ignores presses until its timer passes 45 by holding a
    // flag (0x800CD68C) set. A press clears the flag early. The Acclaim logo
    // (mode 0) is mostly the game loading, so it's left alone.
    void skip_boot_screen() {
        if (trw::controls::rdram == nullptr || *guest_word(0x801407B8) != 1) {
            return;
        }
        *guest_word(0x800CD68C) = 0;
    }

    // The attract demo: a match started from the title screen (every real
    // match starts from the front end's menus). Kept up to date by
    // current_screen(). (The word at 0x80140780 isn't a demo flag: it's 0 in
    // some trial levels too, the Warehouse for one.)
    bool from_title = false;

    // Where the keyboard's Escape and Enter go.
    enum class Screen { Boot, Title, Menu, PauseOptions, PauseList, FlyThrough, Game };
    Screen current_screen() {
        if (trw::controls::rdram == nullptr) {
            return Screen::Menu;
        }
        uint32_t mode = *reinterpret_cast<const uint32_t*>(trw::controls::rdram + 0x1407B8);
        uint32_t paused = *reinterpret_cast<const uint32_t*>(trw::controls::rdram + 0x1407D4);
        uint32_t vi_width = *reinterpret_cast<const uint32_t*>(trw::controls::rdram + 0x14B510);
        if (mode <= 4) {
            return Screen::Boot;
        }
        if (mode == 8 && trw_on_title_screen()) {
            from_title = true;
            return Screen::Title;
        }
        if (mode == 8) {
            from_title = false;
        }
        if (mode == 8 || mode == 20) {
            return Screen::Menu;
        }
        if (from_title) {
            return Screen::Title; // the attract demo: back to the title
        }
        if (paused != 0) {
            // The options opened from the pause list are drawn like the front
            // end's menus, in their 284-pixel video mode.
            return vi_width == 284 ? Screen::PauseOptions : Screen::PauseList;
        }
        // The fly-through at the start of a level: no HUD drawn.
        if (!trw_match_hud_drawn()) {
            return Screen::FlyThrough;
        }
        return Screen::Game;
    }

    bool in_front_end() {
        if (trw::controls::rdram == nullptr) {
            return true;
        }
        uint32_t mode = *reinterpret_cast<const uint32_t*>(trw::controls::rdram + 0x1407B8);
        // The match paused (its pause list and the options opened from it).
        uint32_t paused = *reinterpret_cast<const uint32_t*>(trw::controls::rdram + 0x1407D4);
        return mode <= 4 || mode == 8 || mode == 20 || paused != 0;
    }
}

namespace {
    using Clock = std::chrono::steady_clock;

    // The menu item the cursor points at, or -1 (beside the menu's items,
    // or the cursor hidden). Inside the box around the items, the gaps
    // between them go to the nearest one (a grid's cells are wider than its
    // pictures, a list's rows taller than its text).
    int hovered_item(const trw::menu_cursor::Items& items) {
        int w = window_w.load(), h = window_h.load();
        if (!cursor_shown.load() || w <= 0 || h <= 0 || items.pointable == 0) {
            return -1;
        }
        // The menus fill the 4:3 picture, centred in the window.
        float picture_h = std::min((float)h, w * 0.75f);
        float picture_w = picture_h * 4.0f / 3.0f;
        float mx = (cursor_x.load() - (w - picture_w) * 0.5f) / picture_w * items.width;
        float my = (cursor_y.load() - (h - picture_h) * 0.5f) / picture_h * items.height;
        constexpr float kMargin = 3.0f; // menu pixels around the box
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        int nearest = -1;
        float nearest_distance = 1e9f;
        for (int i = 0; i < items.count; i++) {
            const trw::menu_cursor::Item& item = items.items[i];
            if (!item.pointable) {
                continue;
            }
            x0 = std::min(x0, item.x0);
            y0 = std::min(y0, item.y0);
            x1 = std::max(x1, item.x1);
            y1 = std::max(y1, item.y1);
            float dx = std::max({ item.x0 - mx, 0.0f, mx - item.x1 });
            float dy = std::max({ item.y0 - my, 0.0f, my - item.y1 });
            float distance = dx * dx + dy * dy;
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest = i;
            }
        }
        bool in_box = mx >= x0 - kMargin && mx < x1 + kMargin && my >= y0 - kMargin && my < y1 + kMargin;
        return in_box ? nearest : -1;
    }

    // What a click on an item does once the highlight is on it.
    enum class ClickAction { None, A, Right, Left };

    // The click on an item: A, or Right for a setting with a value or a
    // loadout slot (it steps the value on, as the right direction does);
    // Shift and a click steps it back (Left).
    ClickAction click_action(const trw::menu_cursor::Item& item) {
        if (!item.steps) {
            return ClickAction::A;
        }
        return (SDL_GetModState() & KMOD_SHIFT) ? ClickAction::Left : ClickAction::Right;
    }

    // Moves the game's highlight onto an item by pushing the stick toward
    // it, a short push at a time (the menus move one step per push and skip
    // what can't be picked), then does what a click asked for. Steering goes
    // by the items' positions, so it works for lists and grids alike. A push
    // that doesn't move the highlight marks that direction blocked from
    // there (a grid's locked cells), and the best direction left is tried
    // next; the marks clear once the highlight moves. A push (or press)
    // lasts while the menu is drawn twice, and the next waits until it has
    // been drawn three times more (the pause list wants the stick back in
    // the middle a while), so the game sees each one whatever its frame
    // rate.
    struct MenuSteer {
        static constexpr auto kMinPush = std::chrono::milliseconds(50);
        static constexpr int kPushReads = 3;
        static constexpr uint32_t kPushFrames = 2;
        static constexpr auto kMinGap = std::chrono::milliseconds(60);
        static constexpr uint32_t kGapFrames = 3;
        static constexpr int kMaxPushes = 24;

        // Directions: right, left, down, up (screen), as stick values.
        static constexpr float kDirX[4] = { 1.0f, -1.0f, 0.0f, 0.0f };
        static constexpr float kDirY[4] = { 0.0f, 0.0f, 1.0f, -1.0f };

        uint32_t target = 0;
        ClickAction action = ClickAction::None;
        int pushes = 0;
        uint32_t blocked_from = 0; // the highlight the blocked directions are from
        uint8_t blocked = 0;       // bit per direction
        int last_dir = -1;
        uint32_t last_highlight = 0;

        float push_x = 0.0f, push_y = 0.0f;
        bool push_a = false;
        bool pushing = false;
        int push_reads = 0;
        Clock::time_point push_until{};
        uint32_t pushed_serial = 0;
        uint32_t released_serial = 0;
        Clock::time_point released_at{};

        void start(uint32_t widget, ClickAction then) {
            if (widget != target) {
                pushes = 0;
                last_dir = -1;
                last_highlight = 0;
            }
            target = widget;
            if (then != ClickAction::None) {
                action = then;
            }
        }
        void cancel() {
            target = 0;
            action = ClickAction::None;
        }
        void begin_push(float px, float py, bool a, uint32_t serial) {
            pushing = true;
            pushed_serial = serial;
            push_a = a;
            push_x = px;
            push_y = py;
            push_reads = kPushReads;
            push_until = Clock::now() + kMinPush;
        }
        // (`serial`: the menu's, or 0 where none is known.)
        void press(ClickAction what, uint32_t serial) {
            if (what == ClickAction::Right || what == ClickAction::Left) {
                begin_push(what == ClickAction::Right ? 1.0f : -1.0f, 0.0f, false, serial);
            }
            else if (what == ClickAction::A) {
                begin_push(0.0f, 0.0f, true, serial);
            }
        }

        void step(const trw::menu_cursor::Items& items, bool have_items, uint16_t* buttons, float* x, float* y) {
            if (pushing) {
                bool frames_left = have_items && pushed_serial != 0 && items.serial < pushed_serial + kPushFrames;
                if (push_reads > 0 || Clock::now() < push_until || frames_left) {
                    push_reads--;
                    if (push_a) {
                        *buttons |= BTN_A;
                    }
                    else {
                        *x = push_x;
                        *y = push_y;
                    }
                    return;
                }
                pushing = false;
                released_serial = have_items ? items.serial : 0;
                released_at = Clock::now();
            }
            if (target == 0) {
                return;
            }
            if (!have_items || items.serial < released_serial + kGapFrames || Clock::now() < released_at + kMinGap) {
                return; // waiting for the menu to show the last push
            }
            int at = items.highlighted, goal = -1;
            for (int i = 0; i < items.count; i++) {
                if (items.items[i].widget == target) {
                    goal = i;
                }
            }
            if (goal < 0 || at < 0) {
                cancel();
                return;
            }
            if (goal == at) {
                press(action, items.serial);
                cancel();
                return;
            }
            uint32_t highlight = items.items[at].widget;
            if (highlight != blocked_from) {
                blocked_from = highlight;
                blocked = 0;
            }
            else if (last_dir >= 0 && highlight == last_highlight) {
                blocked |= 1 << last_dir; // the last push went nowhere
            }
            last_highlight = highlight;
            if (pushes >= kMaxPushes) {
                cancel();
                return;
            }
            const trw::menu_cursor::Item& a = items.items[at];
            const trw::menu_cursor::Item& b = items.items[goal];
            float dx = (b.x0 + b.x1 - a.x0 - a.x1) * 0.5f;
            float dy = (b.y0 + b.y1 - a.y0 - a.y1) * 0.5f;
            // The open direction that goes most toward the item; one going
            // away from it only if nothing else is open.
            int best = -1;
            float best_score = -1e9f;
            for (int d = 0; d < 4; d++) {
                if (blocked & (1 << d)) {
                    continue;
                }
                float score = kDirX[d] * dx + kDirY[d] * dy;
                if (score > best_score) {
                    best_score = score;
                    best = d;
                }
            }
            if (best < 0) {
                cancel();
                return;
            }
            pushes++;
            last_dir = best;
            // The stick's y is up.
            begin_push(kDirX[best], -kDirY[best], false, items.serial);
        }
    } menu_steer;

    // A left click selects what it was pressed on while it's held: the
    // highlighted item (A), or another item (steered to, then A once).
    bool click_holds_a = false;

    // Mouse buttons held when play and the menus change over are ignored
    // until let go: the click on Resume doesn't fire, and firing while
    // pausing doesn't pick Resume.
    uint32_t mouse_held_over = 0;
    bool was_in_front_end = true;

    // Each controller read on the mouse's port: the cursor's state.
    void update_menu_mode(bool menus, bool other_input) {
        if (menus != menu_mode.load()) {
            if (menus) {
                int w = 0, h = 0;
                recompui::get_window_size(w, h);
                if (w > 0 && h > 0) {
                    window_w.store(w);
                    window_h.store(h);
                }
                cursor_x.store(w * 0.5f);
                cursor_y.store(h * 0.5f);
            }
            cursor_shown.store(false);
            menu_mode.store(menus);
        }
        if (other_input && cursor_shown.load()) {
            cursor_shown.store(false);
        }
        if (!cursor_shown.load()) {
            menu_steer.cancel();
        }
    }
}

bool trw::controls::menu_cursor(float& x, float& y) {
    int w = window_w.load(), h = window_h.load();
    if (!menu_mode.load() || !cursor_shown.load() || recompinput::game_input_disabled() || w <= 0 || h <= 0) {
        return false;
    }
    x = cursor_x.load() / w;
    y = cursor_y.load() / h;
    return true;
}

void trw::controls::set_keyboard_defaults() {
    auto key = [](GameInput input, std::initializer_list<SDL_Scancode> keys) {
        std::vector<InputField> fields;
        for (SDL_Scancode k : keys) {
            fields.push_back(InputField::keyboard(k));
        }
        recompinput::set_default_mapping_for_keyboard(input, fields);
    };
    // Move and strafe.
    key(GameInput::C_UP, { SDL_SCANCODE_W });
    key(GameInput::C_DOWN, { SDL_SCANCODE_S });
    key(GameInput::C_LEFT, { SDL_SCANCODE_A });
    key(GameInput::C_RIGHT, { SDL_SCANCODE_D });
    // Turn and look from the keyboard (the mouse also does this).
    key(GameInput::Y_AXIS_POS, { SDL_SCANCODE_UP });
    key(GameInput::Y_AXIS_NEG, { SDL_SCANCODE_DOWN });
    key(GameInput::X_AXIS_NEG, { SDL_SCANCODE_LEFT });
    key(GameInput::X_AXIS_POS, { SDL_SCANCODE_RIGHT });
    // Actions.
    key(GameInput::R, { SDL_SCANCODE_SPACE });          // jump
    key(GameInput::Z, { SDL_SCANCODE_LCTRL, SDL_SCANCODE_F }); // fire
    key(GameInput::A, { SDL_SCANCODE_E });              // next weapon / menu accept
    key(GameInput::B, { SDL_SCANCODE_BACKSPACE });      // previous weapon / menu back (Q taps A: see get_input)
    key(GameInput::L, { SDL_SCANCODE_LSHIFT });
    key(GameInput::START, { SDL_SCANCODE_RETURN });
    // The D-pad (taunts); the number keys pick weapons (see get_input).
    key(GameInput::DPAD_UP, { SDL_SCANCODE_Z });
    key(GameInput::DPAD_RIGHT, { SDL_SCANCODE_X });
    key(GameInput::DPAD_DOWN, { SDL_SCANCODE_C });
    key(GameInput::DPAD_LEFT, { SDL_SCANCODE_V });
}

// Controllers: modern dual-stick by default, the left stick moving and
// strafing (the C buttons, Expert controls) and the right stick turning and
// looking (the N64 stick), and the right trigger firing (Z) with the left
// one jumping (R). The input library's own defaults had both the other way
// round. The other buttons as the library has them, and the digital C
// buttons (Y, B, right stick click, right shoulder) kept.
void trw::controls::set_controller_defaults() {
    using recompinput::InputField;
    auto pad = [](GameInput input, std::vector<InputField> fields) {
        recompinput::set_default_mapping_for_controller(input, fields);
    };
    pad(GameInput::Z, { InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) });
    pad(GameInput::R, { InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) });
    pad(GameInput::C_LEFT, { InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, false), InputField::controller_digital(SDL_CONTROLLER_BUTTON_Y) });
    pad(GameInput::C_RIGHT, { InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, true), InputField::controller_digital(SDL_CONTROLLER_BUTTON_B) });
    pad(GameInput::C_UP, { InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, false), InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSTICK) });
    pad(GameInput::C_DOWN, { InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, true), InputField::controller_digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) });
    pad(GameInput::X_AXIS_NEG, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false) });
    pad(GameInput::X_AXIS_POS, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true) });
    pad(GameInput::Y_AXIS_POS, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, false) });
    pad(GameInput::Y_AXIS_NEG, { InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true) });
}

// Once (a marker file beside the controls config): controller profiles saved
// with the library's old defaults (each player's, made before the defaults
// above) get the new ones, binding by binding: a binding is only changed if
// its first field is still the old default's, so ones set by hand stay.
void trw::controls::update_controller_profiles(const std::filesystem::path& config_dir) {
    std::filesystem::path marker = config_dir / "controller_layout_v2";
    std::error_code ec;
    if (std::filesystem::exists(marker, ec)) {
        return;
    }
    using recompinput::InputField;
    struct Old { GameInput input; InputField first; };
    const Old old_defaults[] = {
        { GameInput::Z, InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) },
        { GameInput::R, InputField::controller_analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) },
        { GameInput::C_LEFT, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, false) },
        { GameInput::C_RIGHT, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTX, true) },
        { GameInput::C_UP, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, false) },
        { GameInput::C_DOWN, InputField::controller_analog(SDL_CONTROLLER_AXIS_RIGHTY, true) },
        { GameInput::X_AXIS_NEG, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, false) },
        { GameInput::X_AXIS_POS, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, true) },
        { GameInput::Y_AXIS_POS, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, false) },
        { GameInput::Y_AXIS_NEG, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, true) },
    };
    namespace profiles = recompinput::profiles;
    for (int profile = 0; profile < profiles::get_input_profile_count(); profile++) {
        if (profiles::get_input_profile_device(profile) != recompinput::InputDevice::Controller) {
            continue;
        }
        for (const Old& old : old_defaults) {
            if (profiles::get_input_binding(profile, old.input, 0) == old.first) {
                profiles::reset_input_binding(profile, recompinput::InputDevice::Controller, old.input);
            }
        }
    }
    profiles::save_controls_config(config_dir / "controls.json");
    FILE* f = fopen(marker.string().c_str(), "w");
    if (f != nullptr) {
        fputs("Controller profiles moved to the dual-stick defaults.\n", f);
        fclose(f);
    }
}

void trw::controls::poll_input() {
    recompinput::poll_inputs();
    fresh_poll.store(true);
}

void trw::controls::init() {
    SDL_AddEventWatch(watch_events, nullptr);
}


// From the HUD hook (src/game/widescreen.cpp), so a wheel the game opened
// under the pause menu this frame isn't drawn.
extern "C" void trw_pause_hold_wheels(void) {
    if (trw::controls::rdram != nullptr) {
        hold_wheels_while_paused();
    }
}

// The buttons each port last gave the game (trw::controls::buttons_held).
static std::atomic<uint16_t> last_buttons[4];

static bool get_input_for_port(int player_index, uint16_t* buttons, float* x, float* y);

bool trw::controls::get_input(int player_index, uint16_t* buttons, float* x, float* y) {
    bool ret = get_input_for_port(player_index, buttons, x, y);
    // The mouse's player out of plain mouse look (menus, the pause, the
    // weapon wheel, the team select): no view turning.
    if (player_index == mouse_port()) {
        if (!turn_kept) {
            read_mouse_look(0);
        }
        turn_kept = false;
    }
    // A as the game gets it (the keys' own presses included), for holding
    // the wheel past the pause.
    if (player_index >= 0 && player_index < 4) {
        a_down[player_index] = (*buttons & BTN_A) != 0;
        last_buttons[player_index].store(*buttons);
    }
    return ret;
}

// Frame timing log (PAXDINO_FRAMELOG=1 in the environment): each game frame
// that took over 24 ms from the last (its camera's start to the next), and
// how late its display list went to the renderer, to frametimes.log in the
// working directory, with a line of totals every 10 seconds.
namespace {
    struct FrameLog {
        FILE* file = nullptr;
        bool checked = false;
        std::chrono::steady_clock::time_point start, last_camera, last_sent, last_summary;
        bool have_camera = false, have_sent = false;
        uint32_t frames = 0, late = 0, sent = 0;
        double worst = 0.0, worst_sent = 0.0;
    };
    FrameLog frame_log;

    bool frame_log_on() {
        if (!frame_log.checked) {
            frame_log.checked = true;
            const char* on = std::getenv("PAXDINO_FRAMELOG");
            if (on != nullptr && on[0] != '\0' && on[0] != '0') {
                frame_log.file = std::fopen("frametimes.log", "w");
                frame_log.start = frame_log.last_summary = std::chrono::steady_clock::now();
            }
        }
        return frame_log.file != nullptr;
    }

    double ms_between(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }

    // The game thread, at each frame's camera.
    void log_camera_frame() {
        if (!frame_log_on()) {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        if (frame_log.have_camera) {
            double ms = ms_between(frame_log.last_camera, now);
            frame_log.frames++;
            frame_log.worst = std::max(frame_log.worst, ms);
            if (ms > 24.0) {
                frame_log.late++;
                std::fprintf(frame_log.file, "%10.1f  game frame %6.1f ms  retraces %u\n",
                    ms_between(frame_log.start, now), ms, *guest_word(0x801408A8));
            }
        }
        frame_log.have_camera = true;
        frame_log.last_camera = now;
        if (ms_between(frame_log.last_summary, now) >= 10000.0) {
            std::fprintf(frame_log.file, "%10.1f  -- %u frames (%u to renderer), %u over 24 ms, slowest %.1f ms, slowest to renderer %.1f ms\n",
                ms_between(frame_log.start, now), frame_log.frames, frame_log.sent, frame_log.late, frame_log.worst, frame_log.worst_sent);
            std::fflush(frame_log.file);
            frame_log.frames = frame_log.late = frame_log.sent = 0;
            frame_log.worst = frame_log.worst_sent = 0.0;
            frame_log.last_summary = now;
        }
    }

    // The renderer thread, as a display list is sent.
    void log_sent_frame() {
        if (frame_log.file == nullptr) {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        frame_log.sent++;
        if (frame_log.have_sent) {
            double ms = ms_between(frame_log.last_sent, now);
            frame_log.worst_sent = std::max(frame_log.worst_sent, ms);
            if (ms > 24.0) {
                std::fprintf(frame_log.file, "%10.1f  to renderer %6.1f ms\n", ms_between(frame_log.start, now), ms);
            }
        }
        frame_log.have_sent = true;
        frame_log.last_sent = now;
    }
}

// The camera is built three times a frame in a match (twice from
// func_00236854 at the start of the display list, then from func_00220A5C
// after the sky): only the frame's first takes the mouse motion, so all
// three build the same view.
static uint32_t last_camera_head = 0;

void trw::controls::camera_begin() {
    std::scoped_lock lock(turn_mutex);
    uint32_t head = *guest_word(dl_head_addr);
    bool new_frame = dl_buffer_index(head) != dl_buffer_index(last_camera_head) || head < last_camera_head;
    last_camera_head = head;
    if (!new_frame) {
        return;
    }
    log_camera_frame();
    // The player kept from the last controller read may be the last match's,
    // gone since it was quit (the camera is built while the next one loads).
    if (look_player == 0 || !valid_object(look_player) || !match_live() ||
        !trw_player_live(trw::controls::rdram, look_player)) {
        turn_pending_x = turn_pending_y = 0.0f;
        return;
    }
    // The frame's mouse look: all the motion since the last frame.
    if (turn_pending_x != 0.0f || turn_pending_y != 0.0f) {
        float scale = (float)recompui::config::general::get_mouse_sensitivity() / 100.0f;
        turn_player_by(look_player, turn_pending_x * scale, turn_pending_y * scale);
        turn_pending_x = turn_pending_y = 0.0f;
    }
}

void trw::controls::display_list_sent(uint32_t) {
    std::scoped_lock lock(turn_mutex);
    log_sent_frame();
}

uint16_t trw::controls::buttons_held(int port) {
    return port >= 0 && port < 4 ? last_buttons[port].load() : 0;
}

static bool get_input_for_port(int player_index, uint16_t* buttons, float* x, float* y) {
    bool ret = recompinput::profiles::get_n64_input(player_index, buttons, x, y);
    // The keys' and controllers' own input, before the mouse is added. Shift
    // (L by default) goes with a click (Shift and a click steps a setting
    // back), so it doesn't count.
    bool shift_held = (SDL_GetModState() & KMOD_SHIFT) != 0;
    uint16_t bound_buttons = shift_held ? (uint16_t)(*buttons & ~BTN_L) : *buttons;
    bool bound_input = bound_buttons != 0 || std::fabs(*x) > 0.5f || std::fabs(*y) > 0.5f;
    if (trw::controls::rdram != nullptr && player_index >= 0 && player_index < 4) {
        if (player_index == 0) {
            hold_wheels_while_paused();
        }
        release_a_after_pause(player_index, buttons);
    }

    // The mouse goes with the keyboard's player.
    int port = mouse_port();
    if (port < 0 || recompinput::game_input_disabled()) {
        wheel_steps.store(0);
        mouse_pressed.store(0);
        escape_presses.store(0);
        enter_presses.store(0);
        q_presses.store(0);
        slot_request.store(-1);
        mouse_aims_player.store(0);
        return ret;
    }
    if (player_index != port) {
        return ret;
    }

    uint32_t clicked = mouse_pressed.exchange(0);
    uint32_t mb = mouse_buttons.load() | clicked;
    bool front_end = in_front_end();
    if (front_end != was_in_front_end) {
        mouse_held_over = mouse_buttons.load();
        was_in_front_end = front_end;
        click_holds_a = false;
    }
    mouse_held_over &= mouse_buttons.load();
    clicked &= ~mouse_held_over;
    mb &= ~mouse_held_over;

    // Escape and Enter, by screen. Escape: in a match the pause menu (Start),
    // which it also leaves (resume); in the menus back (B). Enter: select (A)
    // in the menus and the pause list, nothing in a match. The title and the
    // boot screens take Start from either (and on the title Space or a
    // click too).
    // Enter's own Start binding (the defaults, and older saved controls) is
    // dropped while it's held, so the menus don't get both.
    const uint8_t* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) {
        *buttons &= ~BTN_START;
    }
    Screen screen = current_screen();
    {
        const uint8_t* k = keys;
        bool keys_used = bound_input || k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER] || k[SDL_SCANCODE_ESCAPE] ||
            k[SDL_SCANCODE_W] || k[SDL_SCANCODE_A] || k[SDL_SCANCODE_S] || k[SDL_SCANCODE_D];
        bool menus = screen == Screen::Menu || screen == Screen::PauseOptions || screen == Screen::PauseList;
        update_menu_mode(menus && trw::config::get_menu_cursor(), keys_used);
    }
    if (screen == Screen::Title) {
        // The title takes only Start: Enter, Escape, Space or a click.
        if (clicked & SDL_BUTTON_LMASK) {
            space_presses.fetch_add(1);
        }
        if (escape_pulse.step((int)screen) | enter_pulse.step((int)screen) | title_pulse.step((int)screen)) {
            *buttons |= BTN_START;
        }
        return ret;
    }
    if (screen == Screen::FlyThrough) {
        // Start skips it: Escape, Enter, Space or a click. The press ends when
        // the level starts (KeyPulse), so it doesn't go on to pause.
        if (clicked & SDL_BUTTON_LMASK) {
            space_presses.fetch_add(1);
        }
        if (escape_pulse.step((int)screen) | enter_pulse.step((int)screen) | title_pulse.step((int)screen)) {
            *buttons |= BTN_START;
        }
        return ret;
    }
    if (screen == Screen::Boot) {
        // Any button, Enter, Escape, Space or a click skips the boot screens
        // (where they can be skipped).
        if (clicked & SDL_BUTTON_LMASK) {
            space_presses.fetch_add(1);
        }
        if (escape_pulse.step((int)screen) | enter_pulse.step((int)screen) | title_pulse.step((int)screen)) {
            *buttons |= BTN_START;
        }
        if (*buttons != 0 || mb != 0) {
            skip_boot_screen();
        }
        return ret;
    }
    space_presses.store(0);
    bool escape_started = escape_pulse.held_reads() == 0;
    if (escape_pulse.step((int)screen)) {
        // B in the menus. The options opened from the pause list leave
        // through their Resume Game item (ResumeNav); in their sub-screens
        // Escape is B.
        if (screen == Screen::PauseOptions && (resume_nav.active || (escape_started && trw_pause_resume_direction() != 2))) {
            if (escape_started) {
                resume_nav.start();
            }
        }
        else {
            *buttons |= (screen == Screen::Menu || screen == Screen::PauseOptions) ? BTN_B : BTN_START;
        }
    }
    resume_nav.step(screen == Screen::PauseOptions, buttons, y);
    // Enter selects (A) in the menus; in a match it does nothing (Escape
    // pauses).
    if (enter_pulse.step((int)screen) && screen != Screen::Game) {
        *buttons |= BTN_A;
    }

    if (*buttons != 0 || mb != 0) {
        skip_boot_screen();
    }

    if (in_front_end()) {
        // Menus: the arrow keys and WASD (the stick) and the wheel navigate,
        // left click selects (A), right click goes back (B).
        // Mouse motion is left out so the selection doesn't wander.
        int wheel = wheel_pulse.step();
        if (wheel != 0) *y = (float)wheel;
        // WASD navigate like the arrow keys (the stick) instead of being the
        // C buttons; R is C-up, which some screens use (the default loadout,
        // skins).
        constexpr uint16_t BTN_C_UP = 0x0008, BTN_C_DOWN = 0x0004, BTN_C_LEFT = 0x0002, BTN_C_RIGHT = 0x0001;
        if (keys[SDL_SCANCODE_W]) { *buttons &= ~BTN_C_UP; *y = 1.0f; }
        if (keys[SDL_SCANCODE_S]) { *buttons &= ~BTN_C_DOWN; *y = -1.0f; }
        if (keys[SDL_SCANCODE_A]) { *buttons &= ~BTN_C_LEFT; *x = -1.0f; }
        if (keys[SDL_SCANCODE_D]) { *buttons &= ~BTN_C_RIGHT; *x = 1.0f; }
        if (keys[SDL_SCANCODE_R]) { *buttons |= BTN_C_UP; }
        q_presses.store(0);
        slot_request.store(-1);
        pick = WeaponPick{};
        // With the cursor showing (include/trw_menu_cursor.h), pointing at
        // an item moves the highlight onto it and a click on an item selects
        // that item (or, on a setting with a value, steps the value on; with
        // Shift, back). Elsewhere (parts of a screen not in the list found, such
        // as the loadout's OK), or without the cursor, a click selects the
        // highlighted item, as A does.
        if (cursor_shown.load() && shift_held) {
            *buttons &= ~BTN_L; // Shift's L, there for the click
        }
        trw::menu_cursor::Items items;
        bool have_items = cursor_shown.load() && trw::menu_cursor::current(items);
        int hovered = have_items ? hovered_item(items) : -1;
        bool moved = cursor_moved.exchange(false);
        if (wheel != 0) {
            menu_steer.cancel();
        }
        if (moved && hovered >= 0 && hovered != items.highlighted) {
            menu_steer.start(items.items[hovered].widget, ClickAction::None);
        }
        if (clicked & SDL_BUTTON_LMASK) {
            ClickAction what = hovered >= 0 ? click_action(items.items[hovered]) : ClickAction::A;
            if (hovered >= 0 && hovered != items.highlighted) {
                menu_steer.start(items.items[hovered].widget, what);
                click_holds_a = false;
            }
            else {
                // A quick click still lasts long enough to be seen; A is held
                // on while the button is (a setting's value steps once).
                click_holds_a = what == ClickAction::A;
                menu_steer.press(what, have_items ? items.serial : 0);
            }
        }
        if (!(mb & SDL_BUTTON_LMASK)) {
            click_holds_a = false;
        }
        if (click_holds_a) {
            *buttons |= BTN_A;
        }
        // Right click goes back as Escape does: on the options opened from
        // the pause list (not their sub-screens), through Resume Game.
        bool pause_options_top = screen == Screen::PauseOptions && trw_pause_resume_direction() != 2;
        if (pause_options_top) {
            if ((clicked & SDL_BUTTON_RMASK) && !resume_nav.active) {
                resume_nav.start();
                menu_steer.cancel();
            }
        }
        else if (mb & SDL_BUTTON_RMASK) {
            *buttons |= BTN_B;
            menu_steer.cancel();
        }
        menu_steer.step(items, have_items, buttons, x, y);
        return ret;
    }

    // Weapons (see WeaponPick). Controls saved before had 1-4 on the D-pad
    // and Q on B; those are dropped while the keys are held.
    if (keys[SDL_SCANCODE_1] || keys[SDL_SCANCODE_2] || keys[SDL_SCANCODE_3] || keys[SDL_SCANCODE_4]) {
        *buttons &= ~(BTN_DPAD_UP | BTN_DPAD_DOWN | BTN_DPAD_LEFT | BTN_DPAD_RIGHT);
    }
    if (keys[SDL_SCANCODE_Q]) {
        *buttons &= ~BTN_B;
    }
    // R: the weapon's second function (B; "function not available" for
    // weapons without one).
    if (keys[SDL_SCANCODE_R]) {
        *buttons |= BTN_B;
    }
    uint32_t player = match_live() ? player_on_port(player_index) : 0;
    uint32_t wheel_base = player != 0 ? player + wheel_offset : 0;
    // The Raptor's and the Mite's own attacks (weapons 16 and 17, the one
    // held at +0x62E) have no wheel to pick from: holding A for one only
    // waited for a wheel that never opened, with the mouse's look dropped
    // meanwhile (the aim stuck after Q). Weapon keys do nothing then.
    if (player != 0 && pick.phase == 0) {
        uint16_t held = (uint16_t)(*guest_word(player + 0x62C) & 0xFFFF);
        if (held == 16 || held == 17) {
            slot_request.store(-1);
            q_presses.store(0);
            wheel_steps.store(0);
        }
    }
    if (pick.phase == 0 && player != 0) {
        int32_t slot = slot_request.exchange(-1);
        int32_t steps = wheel_steps.load();
        if (slot >= 0) {
            pick = WeaponPick{ 1, 0, 0, slot };
        }
        else if (q_presses.load() > 0) {
            q_presses.fetch_sub(1);
            pick = WeaponPick{ 1, 0, 1, -1 };
        }
        else if (steps != 0) {
            int dir = steps > 0 ? 1 : -1;
            wheel_steps.fetch_sub(dir);
            pick = WeaponPick{ 1, 0, dir, -1 };
        }
    }
    bool wheel_open = wheel_base != 0 && *guest_word(wheel_base) != 0;
    // The team select at the start of a team match: a wheel of four teams,
    // picked by pointing the stick at one and pressing A. Open while the
    // player's +0x11B4 is set and no team is chosen (+0xCC8, -1). The mouse
    // points there instead of turning the view, and a left click is A.
    bool team_select = player != 0 && *guest_word(player + 0x11B4) != 0 && (int32_t)*guest_word(player + 0xCC8) < 0;
    bool relative = SDL_GetRelativeMouseMode() == SDL_TRUE;
    mouse_aims_player.store(relative && !team_select ? player : 0);
    mouse_aims_port.store(player_index);
    bool look = relative && fresh_poll.exchange(false);
    if (pick.phase != 0 && wheel_base == 0) {
        pick = WeaponPick{};
    }
    if (pick.phase != 0) {
        picking_player.store(player);
    }
    else if (!wheel_open) {
        picking_player.store(0);
    }
    if (pick.phase != 0) {
        pick.reads++;
        if (pick.phase == 1) {
            // Open the wheel (stick centred) until it has filled its slots.
            *buttons |= BTN_A;
            *x = *y = 0.0f;
            if (wheel_open && pick.reads >= 3) {
                if (pick.step != 0) {
                    int32_t current = (int32_t)*guest_word(wheel_base + 0x37C);
                    if (current < 0 || current > 7) {
                        current = pick.step > 0 ? 7 : 0;
                    }
                    pick.slot = -1;
                    for (int k = 1; k < 8; k++) {
                        int s8 = ((current + pick.step * k) % 8 + 8) % 8;
                        uint32_t entry = wheel_base + s8 * kWheelSlotSize;
                        if ((int32_t)*guest_word(entry + 0x1C) >= 0 && *guest_word(entry + 0x24) != 0) {
                            pick.slot = s8;
                            break;
                        }
                    }
                }
                pick.phase = pick.slot >= 0 ? 2 : 3;
                pick.reads = 0;
            }
            else if (pick.reads > 12) {
                pick.phase = 3;
                pick.reads = 0;
            }
        }
        if (pick.phase == 2) {
            const float angle = (float)pick.slot * 0.78539816f; // 45 degrees, clockwise from up
            *buttons |= BTN_A;
            *x = sinf(angle);
            *y = cosf(angle);
            if (pick.reads >= 5) {
                pick.phase = 3;
                pick.reads = 0;
            }
        }
        else if (pick.phase == 3) {
            *buttons &= ~BTN_A;
            *x = *y = 0.0f;
            if (pick.reads >= 2) {
                pick = WeaponPick{};
            }
        }
        // The mouse keeps turning the view meanwhile: it sets the angles
        // itself, apart from the stick the wheel is steered by.
        wheel_cursor_x = wheel_cursor_y = 0.0f;
        mouse_look(player_index, player, look);
    }
    else if (team_select) {
        if (look) {
            float dx = 0.0f, dy = 0.0f;
            recompinput::get_mouse_deltas(&dx, &dy);
            wheel_cursor_x = std::clamp(wheel_cursor_x + dx * 0.02f, -1.0f, 1.0f);
            wheel_cursor_y = std::clamp(wheel_cursor_y - dy * 0.02f, -1.0f, 1.0f);
            float len = sqrtf(wheel_cursor_x * wheel_cursor_x + wheel_cursor_y * wheel_cursor_y);
            if (len > 1.0f) {
                wheel_cursor_x /= len;
                wheel_cursor_y /= len;
            }
        }
        // Only once the mouse has pointed somewhere, so a controller's stick
        // still works.
        if (wheel_cursor_x * wheel_cursor_x + wheel_cursor_y * wheel_cursor_y > 0.25f) {
            *x = wheel_cursor_x;
            *y = wheel_cursor_y;
        }
    }
    else if (wheel_open && (*buttons & BTN_A)) {
        if (look) {
            float dx = 0.0f, dy = 0.0f;
            recompinput::get_mouse_deltas(&dx, &dy);
            wheel_cursor_x = std::clamp(wheel_cursor_x + dx * 0.02f, -1.0f, 1.0f);
            wheel_cursor_y = std::clamp(wheel_cursor_y - dy * 0.02f, -1.0f, 1.0f);
            float len = sqrtf(wheel_cursor_x * wheel_cursor_x + wheel_cursor_y * wheel_cursor_y);
            if (len > 1.0f) {
                wheel_cursor_x /= len;
                wheel_cursor_y /= len;
            }
        }
        *x = wheel_cursor_x;
        *y = wheel_cursor_y;
    }
    else {
        wheel_cursor_x = wheel_cursor_y = 0.0f;
        mouse_look(player_index, player, look);
    }

    // On the team select a click is held as A for a moment at least (a
    // quick one only marked the team, without choosing it).
    static Clock::time_point team_a_until{};
    if (team_select && (clicked & SDL_BUTTON_LMASK)) {
        team_a_until = Clock::now() + std::chrono::milliseconds(250);
    }
    if (team_select && Clock::now() < team_a_until) {
        *buttons |= BTN_A;
    }
    if (mb & SDL_BUTTON_LMASK) *buttons |= team_select ? BTN_A : BTN_Z;
    // Right click: the weapon's second function (B), as R.
    if (mb & SDL_BUTTON_RMASK) *buttons |= BTN_B;
    return ret;
}

// Before the HUD draws the weapon wheel (turokrw.us.toml): 1 to skip it while
// it's only open for a pick by Q, the mouse wheel or the number keys.
extern "C" int trw_wheel_hidden(uint8_t*, uint32_t player) {
    return player != 0 && player == picking_player.load() ? 1 : 0;
}

// The game's look code eases each player's pitch toward the angle its look
// input points at (func_002231B0 and others; the input at +0x6A0, back to
// level with the stick centred while the game's look spring is on, held
// where it was left with it off). Mouse look sets the pitch itself and
// doesn't move that input, so the pitch was pulled back (the game's own
// behaviour, the view going back to level). With Look Spring Override on
// (Controls tab), the easing aims at the pitch the mouse set for the
// mouse's player.
// From turokrw.us.toml before each easing of +0x724: the target to use.
extern "C" uint32_t trw_look_pitch_target(uint8_t* rdram, uint32_t player, uint32_t target) {
    // The game's target's last value for the mouse's player: its changes
    // (look keys, the stick) still move the view, from where the mouse left it.
    static uint32_t last_player = 0;
    static float last_target = 0.0f;
    float game_target;
    std::memcpy(&game_target, &target, sizeof(float));
    if (player == 0 || player != mouse_aims_player.load() || !trw::config::get_look_spring_override(mouse_aims_port.load()) || !match_live()) {
        if (player == last_player) {
            last_player = 0;
        }
        return target;
    }
    if (player != last_player) {
        last_player = player;
        last_target = game_target;
    }
    float pitch = read_float(player + player_pitch_offset) + (game_target - last_target);
    last_target = game_target;
    pitch = std::clamp(pitch, -pitch_limit, pitch_limit);
    uint32_t bits;
    std::memcpy(&bits, &pitch, sizeof(float));
    return bits;
}
