#ifndef __TRW_MENU_CURSOR_H__
#define __TRW_MENU_CURSOR_H__

#include <cstdint>

// The mouse cursor in the game's own menus: the items under it are found in
// the menus' widgets (src/game/widescreen.cpp), the game's highlight is
// steered onto the one pointed at (src/main/fps_controls.cpp), and the
// cursor is drawn over the picture (src/main/menu_cursor.cpp).
namespace trw::menu_cursor {
    constexpr int kMaxItems = 48;

    // A menu's items that the highlight moves between: the highlighted
    // widget and its siblings. Boxes are in the menu's own pixels (`width` x
    // `height`, the 4:3 picture).
    struct Item {
        uint32_t widget;
        uint16_t type;  // the widget's
        bool pointable; // shown and not greyed out; the highlight can also
                        // be on a hidden one (a grid's locked cell)
        bool steps;     // a click steps its value (as Right) instead of A
        float x0, y0, x1, y1;
    };
    // Items whose value Right (and Left) change: the options' settings with
    // a value to cycle through (Blood Color, 2 Player Layout, ...) and their
    // sliders (Brightness); also the loadout's weapon slots (src/game/
    // widescreen.cpp).
    constexpr uint16_t kValueItem = 0xB64;
    constexpr uint16_t kSliderItem = 0xB63;
    struct Items {
        int count = 0;
        int pointable = 0; // how many can be pointed at
        int highlighted = -1;
        float width = 284.0f;
        float height = 222.0f;
        uint32_t serial = 0; // goes up with each menu frame published
        Item items[kMaxItems];
    };

    // From the menu hook, once per menu drawn that has such items (the game
    // thread).
    void publish(const Items& items);
    // The items of the last menu drawn, if one was drawn in the last moments.
    bool current(Items& out);

    // At the start of each game frame: makes the overlay the cursor is drawn
    // on (the cursor moves itself on the UI thread).
    void frame_start();
}

#endif
