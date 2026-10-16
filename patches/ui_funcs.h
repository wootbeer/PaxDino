#ifndef TRW_PATCHES_UI_FUNCS_H
#define TRW_PATCHES_UI_FUNCS_H

// RecompFrontend's recompui includes this file from the game's patches/
// folder (src/api/ui_api_events.cpp), so it has to exist.
#include "recompui_event_structs.h"

#include "patch_helpers.h"

DECLARE_FUNC(void, recomp_run_ui_callbacks);

#endif // TRW_PATCHES_UI_FUNCS_H
