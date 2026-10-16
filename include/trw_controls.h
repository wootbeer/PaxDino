#ifndef __TRW_CONTROLS_H__
#define __TRW_CONTROLS_H__

#include <cstdint>
#include <filesystem>

// Mouse and keyboard controls (src/main/fps_controls.cpp).
namespace trw::controls {
    // Default keyboard bindings. Call before the config tabs are created.
    void set_keyboard_defaults();
    // Default controller bindings (modern dual-stick). Call with the above.
    void set_controller_defaults();
    // Once: saved controller profiles still on the old defaults get the new
    // ones. Call after the controls config is loaded.
    void update_controller_profiles(const std::filesystem::path& config_dir);
    // Starts tracking mouse buttons and the wheel. Call once SDL is up.
    void init();
    // ultramodern input callbacks: polls the inputs, then the player's
    // bindings plus mouse look.
    void poll_input();
    bool get_input(int player_index, uint16_t* buttons, float* x, float* y);
    // The N64 buttons the game last read on a controller port (0-3), the
    // keyboard and mouse included. 0 for other ports.
    uint16_t buttons_held(int port);
    // The mouse cursor in the game's menus (include/trw_menu_cursor.h):
    // true while it shows, with its position as a fraction of the window.
    bool menu_cursor(float& x, float& y);
    // Set at boot (tlb_views.cpp) so the controls can read the game's state.
    extern uint8_t* rdram;
}

#endif
