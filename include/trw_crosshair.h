#ifndef __TRW_CROSSHAIR_H__
#define __TRW_CROSSHAIR_H__

// Custom Crosshair (Enhancements tab, src/main/crosshair.cpp): the game's
// crosshair replaced by one drawn over the picture by the UI.
namespace trw::crosshair {
    // At the start of each game frame (the game thread): shows the crosshairs
    // the game drew in the last frame, in the style chosen.
    void frame_start(bool in_match);
    // Choose Image (the button under Crosshair; the UI thread): a file
    // dialog, then the picture is kept for the Custom Image style.
    void choose_image();
    // The pictures of the styles for the Crosshair option's description: the
    // markup (any time), and the pictures it shows (once the UI is up).
    const char* preview_rml();
    void queue_previews();
}

#endif
