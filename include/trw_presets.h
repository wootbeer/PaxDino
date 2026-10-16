#ifndef __TRW_PRESETS_H__
#define __TRW_PRESETS_H__

namespace recompui {
    class GameOptionsMenu;
}

// The launcher's Preset entry (src/main/presets.cpp): Classic or Modern,
// each setting a fixed list of options and leaving the rest alone.
namespace trw::presets {
    // From the launcher's init callback, after Settings.
    void add_launcher_option(recompui::GameOptionsMenu* options);
    // Before recompui::config::finalize(): whether this is the first launch
    // (no saved graphics settings yet).
    void note_first_launch();
    // After finalize(): on the first launch, Modern for everything on its list
    // (the frontend's own graphics defaults differ).
    void apply_first_launch();
}

#endif
