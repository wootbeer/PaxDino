#ifndef __TRW_MUSIC_H__
#define __TRW_MUSIC_H__

#include <cstddef>
#include <cstdint>
#include <filesystem>

// Custom Music (Enhancements tab): the game's songs replaced by the player's
// own files from the Music folder (src/main/custom_music.cpp).
namespace trw::music {
    // The Music folder beside the executable; created at startup if missing.
    std::filesystem::path folder();
    void create_folder();

    // The options, refreshed once a frame by the game thread.
    void set_options(bool enabled, bool keep_song, float volume);
    // Whether a match is on and paused, once a frame (the game thread).
    void set_game_state(bool in_match, bool paused);
    // Randomize Game Music (Enhancements tab), refreshed once a frame.
    void set_randomize(bool enabled);
    // Opens the Music folder in the system's file browser.
    void open_folder();

    // Adds the custom track to `frames` stereo frames of the game's output
    // at `rate` Hz (the audio thread, src/main/main.cpp). `scale` is the
    // main volume.
    void mix(float* stereo, size_t frames, uint32_t rate, float scale);
}

#endif
