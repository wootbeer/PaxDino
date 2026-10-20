#ifndef TRW_RECOMP_HOOKS_H
#define TRW_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in turokrw.us.toml needs beyond
// what N64Recomp's own generated recomp.h already provides (ctx,
// S32/S64/U32/U64, lo/hi).

#include <stdint.h>

// ultramodern (the runtime's own osViSetMode)
void osViSetMode(uint8_t* rdram, int32_t mode);

// src/game/pi_dma.cpp
void trw_set_pi_event(uint8_t* rdram, uint64_t mq, uint32_t msg);

// src/game/widescreen.cpp
int trw_widescreen_active(void);
void trw_border_begin(uint8_t* rdram);
void trw_border_end(uint8_t* rdram);
void trw_hud_begin(uint8_t* rdram, uint64_t view);
void trw_hud_end(uint8_t* rdram);
void trw_hud_anchor(uint8_t* rdram, int on);
int trw_wheel_hidden(uint8_t* rdram, uint32_t player);
void trw_kill_name_colour(uint8_t* rdram, uint64_t entry);
void trw_frame_buffers_set(uint8_t* rdram);
void trw_fade_begin(uint8_t* rdram, uint64_t sp);
void trw_menu_widget_begin(uint8_t* rdram, uint64_t widget, uint64_t sp);
void trw_menu_widget_end(uint8_t* rdram, uint64_t sp);
void trw_menu_image_begin(uint8_t* rdram, uint64_t widget);
void trw_menu_image_end(uint8_t* rdram);

// src/main/extras_config.cpp: 1 while an option of the Enhancements, Unlocks
// or Cheats tab is on (trw::config::Option).
int trw_option(int option);

// src/game/extras.cpp: 1 to leave out a menu image (the scanline strips while
// the Enhancements tab's option is on).
int trw_menu_image_hidden(uint8_t* rdram, uint64_t widget);

// src/game/extras.cpp: 1 to answer "set" from the bit test for a profile's
// level bits while the Unlocks tab's All Levels is on.
int trw_unlocked_bit(uint8_t* rdram, uint64_t bits, uint64_t n);

// src/game/extras.cpp: a character's skin count for the skin change, the
// full count while the Unlocks tab's All Skins is on.
uint32_t trw_skin_count(uint8_t* rdram, uint64_t offset, uint64_t count);

// src/game/extras.cpp: the camera's field of view (fovy, float bits) with
// the Graphics tab's Field of View applied.
uint32_t trw_field_of_view(uint8_t* rdram, uint32_t fovy_bits);

// src/game/extras.cpp: the game's camera (func_00234FCC) starts.
void trw_camera_begin(uint8_t* rdram);

// src/game/extras.cpp: the frame step (float bits), for 60 frames a second
// in matches.
uint32_t trw_frame_step(uint8_t* rdram, uint32_t step_bits);
// src/game/extras.cpp: game frames to one of the game's own 30 a second (2
// at 60 frames a second, else 1), for timers it counts in frames.
int trw_frames_scale(uint8_t* rdram);

// src/main/custom_music.cpp: a song starts on the music player `seqp`; the
// volume to give a sequence player (0 while a file replaces its song); a
// sequence player stops.
void trw_music_song(uint8_t* rdram, uint32_t song, uint32_t seqp);
int32_t trw_music_seqp_vol(uint8_t* rdram, uint32_t seqp, int32_t vol);
void trw_music_seqp_stop(uint8_t* rdram, uint32_t seqp);
// src/main/custom_music.cpp: the song to play for a request (Randomize Game
// Music changes a match's).
int32_t trw_music_request(uint8_t* rdram, int32_t song);

// src/main/crosshair.cpp: 1 to leave out a HUD sprite (the game's crosshair,
// while another style is chosen).
int trw_crosshair_sprite(uint8_t* rdram, uint64_t image, uint64_t x, uint64_t y, uint64_t sp);

// src/game/weapon_rules.cpp: the weapon to switch to (Loadout Restrictions,
// Enhancements tab).
int32_t trw_allowed_weapon(uint8_t* rdram, uint64_t player, int32_t weapon);
// 1 to draw a HUD ammo counter (0 bullet, 1 energy, 2 explosive).
int trw_ammo_counter_shown(int kind);
// A player (re)spawning: the allowed kind's starting ammo.
void trw_starting_ammo(uint8_t* rdram, uint64_t player);

// src/game/extras.cpp: around the trials map's build (1 at the start, 0 at
// the end), which reads the profile's real level bits.
void trw_trial_map_build(int building);

// src/game/weapon_reverb.cpp: Weapon Reverb (Sound tab). Before a call from
// the effects' and projectiles' code, or from the players' movement code; a
// sound effect asked for and the request's end; a sound's reverb amount as
// it starts.
void trw_sfx_world_site(void);
void trw_sfx_local_site(void);
void trw_sfx_request_begin(int32_t effect);
void trw_sfx_request_end(void);
int32_t trw_sfx_fx_mix(int32_t fx_mix);

// src/main/fps_controls.cpp: the pitch the game's look code eases `player`'s
// toward (`target`, bits of a float), or the pitch the mouse set (Look Spring
// Override on, Controls tab).
uint32_t trw_look_pitch_target(uint8_t* rdram, uint32_t player, uint32_t target);

// src/game/stock_runtime_compat.cpp: osClockRate as osInitialize leaves it.
void trw_set_clock_rate(uint8_t* rdram);

#endif // TRW_RECOMP_HOOKS_H
