// Custom Music (Enhancements tab): while it's on and the Music folder beside
// the executable has .mp3, .flac, .wav or .ogg files, each song the game
// starts is replaced by one of them, picked at random (all of them once
// before any repeats), and the next one follows when a file ends. A file
// started in a match plays on until the match is over (the pause menu's
// options switch to the menu song and back); with Keep Song on, a file
// plays on through every song change until it ends.
//
// The game plays its songs on one sequence player (the music object at
// 0x80107DE4; its player at +0x14). Hooks in turokrw.us.toml tell this file
// when a song starts (trw_music_song), when the game sets the player's volume
// (trw_music_seqp_vol, which then gives the game 0 so its song is silent; the
// volumes it asks for, which carry its fades and pause ducking, scale the
// file instead) and when it stops it (trw_music_seqp_stop). The file is mixed
// into the game's audio output (trw::music::mix, from src/main/main.cpp).
#include "trw_music.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "SDL.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"

namespace fs = std::filesystem;

namespace {
    // ------------------------------------------------------------ decoding

    // A file being played: stereo float frames at its own sample rate.
    class Track {
    public:
        virtual ~Track() = default;
        // Fills `out` with up to `frames` stereo frames; fewer at the end.
        virtual size_t read(float* out, size_t frames) = 0;
        uint32_t rate = 44100;

    protected:
        std::vector<char> data;
        uint32_t channels = 2;
        std::vector<float> scratch;

        // Reads `frames` frames of `channels` channels with `read_raw`, then
        // folds them to stereo (mono is doubled, extra channels dropped).
        template <typename F>
        size_t read_folded(float* out, size_t frames, F read_raw) {
            if (channels == 2) {
                return read_raw(out, frames);
            }
            scratch.resize(frames * channels);
            size_t got = read_raw(scratch.data(), frames);
            for (size_t i = 0; i < got; i++) {
                float l = scratch[i * channels];
                float r = channels > 1 ? scratch[i * channels + 1] : l;
                out[i * 2] = l;
                out[i * 2 + 1] = r;
            }
            return got;
        }
    };

    class Mp3Track : public Track {
        drmp3 mp3{};
        bool ok = false;
    public:
        explicit Mp3Track(std::vector<char>&& bytes) {
            data = std::move(bytes);
            ok = drmp3_init_memory(&mp3, data.data(), data.size(), nullptr);
            if (ok) {
                rate = mp3.sampleRate;
                channels = mp3.channels;
            }
        }
        ~Mp3Track() override { if (ok) drmp3_uninit(&mp3); }
        bool valid() const { return ok && channels > 0 && rate > 0; }
        size_t read(float* out, size_t frames) override {
            return read_folded(out, frames, [this](float* dst, size_t n) { return (size_t)drmp3_read_pcm_frames_f32(&mp3, n, dst); });
        }
    };

    class FlacTrack : public Track {
        drflac* flac = nullptr;
    public:
        explicit FlacTrack(std::vector<char>&& bytes) {
            data = std::move(bytes);
            flac = drflac_open_memory(data.data(), data.size(), nullptr);
            if (flac != nullptr) {
                rate = flac->sampleRate;
                channels = flac->channels;
            }
        }
        ~FlacTrack() override { if (flac != nullptr) drflac_close(flac); }
        bool valid() const { return flac != nullptr && channels > 0 && rate > 0; }
        size_t read(float* out, size_t frames) override {
            return read_folded(out, frames, [this](float* dst, size_t n) { return (size_t)drflac_read_pcm_frames_f32(flac, n, dst); });
        }
    };

    class WavTrack : public Track {
        drwav wav{};
        bool ok = false;
    public:
        explicit WavTrack(std::vector<char>&& bytes) {
            data = std::move(bytes);
            ok = drwav_init_memory(&wav, data.data(), data.size(), nullptr);
            if (ok) {
                rate = wav.sampleRate;
                channels = wav.channels;
            }
        }
        ~WavTrack() override { if (ok) drwav_uninit(&wav); }
        bool valid() const { return ok && channels > 0 && rate > 0; }
        size_t read(float* out, size_t frames) override {
            return read_folded(out, frames, [this](float* dst, size_t n) { return (size_t)drwav_read_pcm_frames_f32(&wav, n, dst); });
        }
    };

    class OggTrack : public Track {
        stb_vorbis* vorbis = nullptr;
    public:
        explicit OggTrack(std::vector<char>&& bytes) {
            data = std::move(bytes);
            int error = 0;
            vorbis = stb_vorbis_open_memory(reinterpret_cast<const unsigned char*>(data.data()), (int)data.size(), &error, nullptr);
            if (vorbis != nullptr) {
                stb_vorbis_info info = stb_vorbis_get_info(vorbis);
                rate = info.sample_rate;
                channels = info.channels;
            }
        }
        ~OggTrack() override { if (vorbis != nullptr) stb_vorbis_close(vorbis); }
        bool valid() const { return vorbis != nullptr && rate > 0; }
        size_t read(float* out, size_t frames) override {
            // stb_vorbis folds to two channels itself.
            return (size_t)stb_vorbis_get_samples_float_interleaved(vorbis, 2, out, (int)(frames * 2));
        }
    };

    std::string lower_extension(const fs::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return ext;
    }

    bool is_music_file(const fs::path& path) {
        std::string ext = lower_extension(path);
        return ext == ".mp3" || ext == ".flac" || ext == ".wav" || ext == ".ogg";
    }

    template <typename T>
    std::unique_ptr<Track> make_track(std::vector<char>&& bytes) {
        auto track = std::make_unique<T>(std::move(bytes));
        if (!track->valid()) {
            return nullptr;
        }
        return track;
    }

    std::unique_ptr<Track> open_track(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return nullptr;
        }
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.empty()) {
            return nullptr;
        }
        std::string ext = lower_extension(path);
        if (ext == ".mp3") return make_track<Mp3Track>(std::move(bytes));
        if (ext == ".flac") return make_track<FlacTrack>(std::move(bytes));
        if (ext == ".wav") return make_track<WavTrack>(std::move(bytes));
        if (ext == ".ogg") return make_track<OggTrack>(std::move(bytes));
        return nullptr;
    }

    std::vector<fs::path> list_music_files() {
        std::vector<fs::path> files;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(trw::music::folder(), ec)) {
            if (entry.is_regular_file(ec) && is_music_file(entry.path())) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // ------------------------------------------------------------- state

    std::atomic<bool> option_enabled{ false };
    std::atomic<bool> option_keep_song{ false };
    std::atomic<float> option_volume{ 0.6f };

    // Matches, counted as they start (trw::music::set_game_state), so a file
    // started in a match can be kept until the match is over.
    std::atomic<bool> in_match{ false };
    std::atomic<bool> match_paused{ false };
    std::atomic<uint32_t> match_serial{ 0 };

    std::mutex mutex;                 // everything below
    std::unique_ptr<Track> track;     // the file playing, if any
    bool active = false;              // a game song is being replaced
    bool loading = false;
    uint32_t load_generation = 0;
    uint32_t music_seqp = 0;          // the game's music sequence player
    int64_t current_song = -2;
    int start_volume = 0;             // the game's volume when the song started
    float target_gain = 1.0f;
    float gain = 0.0f;
    uint32_t file_match = 0;          // the match the file was started in, 0 for none
    std::vector<fs::path> shuffle_bag;
    fs::path last_file;
    std::mt19937 rng{ std::random_device{}() };

    // Linear resampling from the file's rate to the game's.
    std::vector<float> chunk;         // decoded frames waiting to be used
    size_t chunk_pos = 0;
    float prev_frame[2] = { 0, 0 };
    float next_frame[2] = { 0, 0 };
    double frac = 1.0;

    fs::path next_file_locked() {
        if (shuffle_bag.empty()) {
            shuffle_bag = list_music_files();
            std::shuffle(shuffle_bag.begin(), shuffle_bag.end(), rng);
            if (shuffle_bag.size() > 1 && shuffle_bag.back() == last_file) {
                std::swap(shuffle_bag.front(), shuffle_bag.back());
            }
        }
        if (shuffle_bag.empty()) {
            return {};
        }
        fs::path path = shuffle_bag.back();
        shuffle_bag.pop_back();
        last_file = path;
        return path;
    }

    void reset_resampler_locked() {
        chunk.clear();
        chunk_pos = 0;
        prev_frame[0] = prev_frame[1] = 0;
        next_frame[0] = next_frame[1] = 0;
        frac = 1.0;
    }

    // Opens the next file on a thread of its own (a big FLAC takes a moment
    // to read), then hands it over if nothing newer was asked for meanwhile.
    void start_loading_locked() {
        track.reset();
        reset_resampler_locked();
        loading = true;
        uint32_t generation = ++load_generation;
        std::thread([generation] {
            std::unique_ptr<Track> loaded;
            for (int attempt = 0; attempt < 8 && loaded == nullptr; attempt++) {
                fs::path path;
                {
                    std::lock_guard lock{ mutex };
                    if (generation != load_generation) {
                        return;
                    }
                    path = next_file_locked();
                }
                if (path.empty()) {
                    break;
                }
                loaded = open_track(path);
            }
            std::lock_guard lock{ mutex };
            if (generation != load_generation) {
                return;
            }
            loading = false;
            if (loaded != nullptr && active) {
                track = std::move(loaded);
                reset_resampler_locked();
            }
        }).detach();
    }

    // The next source frame into next_frame; false at the end of the file.
    bool pull_frame_locked() {
        if (chunk_pos >= chunk.size()) {
            constexpr size_t chunk_frames = 1024;
            chunk.resize(chunk_frames * 2);
            size_t got = track->read(chunk.data(), chunk_frames);
            chunk.resize(got * 2);
            chunk_pos = 0;
            if (got == 0) {
                return false;
            }
        }
        next_frame[0] = chunk[chunk_pos];
        next_frame[1] = chunk[chunk_pos + 1];
        chunk_pos += 2;
        return true;
    }
}

fs::path trw::music::folder() {
    static const fs::path path = [] {
        fs::path base;
        // Beside the AppImage on Linux: its own folder is read-only.
        if (const char* appimage = std::getenv("APPIMAGE")) {
            return fs::path(appimage).parent_path() / "Music";
        }
        if (char* sdl_base = SDL_GetBasePath()) {
            base = fs::path(std::u8string(reinterpret_cast<const char8_t*>(sdl_base))); // UTF-8
            SDL_free(sdl_base);
        }
        return base / "Music";
    }();
    return path;
}

void trw::music::create_folder() {
    std::error_code ec;
    fs::create_directories(folder(), ec);
}

void trw::music::set_options(bool enabled, bool keep_song, float volume) {
    option_volume.store(volume);
    option_keep_song.store(keep_song);
    if (option_enabled.exchange(enabled) && !enabled) {
        // Turned off: the file fades out; the game's own music comes back
        // with its next song.
        std::lock_guard lock{ mutex };
        active = false;
        current_song = -2;
        target_gain = 0.0f;
        ++load_generation;
        loading = false;
    }
}

void trw::music::set_game_state(bool match, bool paused) {
    if (match && !in_match.load()) {
        match_serial.fetch_add(1);
    }
    in_match.store(match);
    match_paused.store(match && paused);
}

void trw::music::open_folder() {
    create_folder();
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", folder().wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::string command = "xdg-open \"" + folder().string() + "\" >/dev/null 2>&1 &";
    int result = std::system(command.c_str());
    (void)result;
#endif
}

void trw::music::mix(float* stereo, size_t frames, uint32_t rate, float scale) {
    std::lock_guard lock{ mutex };
    if (track == nullptr || rate == 0) {
        return;
    }
    const double step = (double)track->rate / rate;
    const float volume = option_volume.load() * scale;
    const float smoothing = 1.0f - std::exp(-1.0f / (0.05f * rate)); // about 50 ms
    for (size_t i = 0; i < frames; i++) {
        while (frac >= 1.0) {
            prev_frame[0] = next_frame[0];
            prev_frame[1] = next_frame[1];
            if (!pull_frame_locked()) {
                // The file ended: the next one follows while the song lasts.
                if (active) {
                    start_loading_locked();
                }
                else {
                    track.reset();
                    reset_resampler_locked();
                }
                return;
            }
            frac -= 1.0;
        }
        gain += (target_gain - gain) * smoothing;
        float t = (float)frac;
        float level = gain * volume;
        stereo[i * 2] += (prev_frame[0] + (next_frame[0] - prev_frame[0]) * t) * level;
        stereo[i * 2 + 1] += (prev_frame[1] + (next_frame[1] - prev_frame[1]) * t) * level;
        frac += step;
    }
    if (!active && gain < 0.001f) {
        track.reset(); // faded out after a stop
        reset_resampler_locked();
    }
}

// ---------------------------------------------------- hooks (game thread)

// A song starts (func_0025D3F0, the music object's start state): `seqp` is
// its sequence player.
extern "C" void trw_music_song(uint8_t*, uint32_t song, uint32_t seqp) {
    if (!option_enabled.load()) {
        return;
    }
    std::lock_guard lock{ mutex };
    bool playing = track != nullptr || loading;
    uint32_t match = in_match.load() ? match_serial.load() : 0;
    // The file playing carries on (still replacing the game's song) when
    // the same song is asked for again (a retried load), when Keep Song is
    // on, or within the match it started in (the pause menu's options play
    // the menu song, and the match's comes back after).
    bool keep = playing && music_seqp == seqp &&
        ((active && current_song == (int64_t)song) || option_keep_song.load() || (match != 0 && file_match == match));
    if (keep) {
        active = true;
        current_song = song;
        target_gain = 1.0f;
        return;
    }
    if (list_music_files().empty()) {
        active = false;
        return;
    }
    music_seqp = seqp;
    current_song = song;
    active = true;
    start_volume = 0;
    target_gain = 1.0f;
    gain = 0.0f;
    file_match = match;
    start_loading_locked();
}

// n_alSeqpSetVol(seqp, vol): the volume to give the player (0 while a file
// replaces its song).
extern "C" int32_t trw_music_seqp_vol(uint8_t*, uint32_t seqp, int32_t vol) {
    if (!option_enabled.load()) {
        return vol;
    }
    std::lock_guard lock{ mutex };
    if (!active || seqp != music_seqp) {
        return vol;
    }
    if (start_volume <= 0) {
        start_volume = std::max(vol, 1);
    }
    // Keep Song plays at a steady level; the pause menu's songs within a
    // match don't move it either.
    if (!option_keep_song.load() && !match_paused.load()) {
        target_gain = std::clamp((float)vol / start_volume, 0.0f, 1.5f);
    }
    return 0;
}

// n_alSeqpStop(seqp): the song ends, so does the file (faded out).
extern "C" void trw_music_seqp_stop(uint8_t*, uint32_t seqp) {
    std::lock_guard lock{ mutex };
    if (!active || seqp != music_seqp) {
        return;
    }
    // Keep Song, or a stop within the match the file belongs to (the pause
    // menu's options): the file plays on.
    uint32_t match = in_match.load() ? match_serial.load() : 0;
    if (option_keep_song.load() || (match != 0 && file_match == match)) {
        return;
    }
    active = false;
    current_song = -2;
    target_gain = 0.0f;
    ++load_generation;
    loading = false;
}

// ------------------------------------------- Randomize Game Music (hook)

// The game's own songs that loop and suit a match (by ear: a
// level's song is 56-59, picked at random by the game; 50 and 60 are its
// other looping songs; 52 is the menu's; the rest stop after a while).
static constexpr int32_t kMatchSongs[] = { 50, 56, 57, 58, 59, 60 };
static std::atomic<bool> option_randomize{ false };

void trw::music::set_randomize(bool enabled) {
    option_randomize.store(enabled);
}

// func_0025E274(song), the game asking for a song: a match's song (56-59)
// becomes any of the match songs, never the one picked last.
extern "C" int32_t trw_music_request(uint8_t*, int32_t song) {
    if (!option_randomize.load() || song < 56 || song > 59) {
        return song;
    }
    static int32_t last = -1;
    static std::mt19937 pick_rng{ std::random_device{}() };
    constexpr size_t count = sizeof(kMatchSongs) / sizeof(kMatchSongs[0]);
    int32_t pick = song;
    for (int tries = 0; tries < 8; tries++) {
        pick = kMatchSongs[pick_rng() % count];
        if (pick != last) {
            break;
        }
    }
    last = pick;
    return pick;
}
