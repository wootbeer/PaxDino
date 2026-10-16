#ifndef __TRW_LAUNCHER_ART_H__
#define __TRW_LAUNCHER_ART_H__

namespace recompui {
    class LauncherMenu;
}

// The launcher's moving jungle background (src/main/launcher_art.cpp).
namespace trw::launcher_art {
    // From the launcher's init callback.
    void init(recompui::LauncherMenu* menu);
    // From the launcher's update callback, every frame.
    void update();
}

#endif
