// Input record and replay (a step toward netplay): with PAXDINO_INPUTREC set
// to a file path, what each controller read hands the game (each port's
// buttons and stick, in the order the game reads them) is written there;
// with PAXDINO_INPUTPLAY set to such a file, the game is given that instead
// of the live input. Run with the sync test (PAXDINO_SYNCTEST, the runtime's
// deterministic mode), a replay should hash the same as the run recorded.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "trw_input_record.h"

namespace {
    struct Read {
        int32_t port;
        int32_t response; // the controller answered
        uint32_t buttons;
        float x, y;
        trw::inputrec::Extras extras;
    };

    FILE* record_file = nullptr;
    FILE* play_file = nullptr;
    bool checked = false;
    bool play_ended = false;

    void check() {
        if (checked) {
            return;
        }
        checked = true;
        if (const char* path = std::getenv("PAXDINO_INPUTPLAY"); path != nullptr && path[0] != '\0') {
            play_file = std::fopen(path, "rb");
            if (play_file == nullptr) {
                std::fprintf(stderr, "[TRW] input replay: can't read %s\n", path);
            }
        }
        else if (const char* path = std::getenv("PAXDINO_INPUTREC"); path != nullptr && path[0] != '\0') {
            record_file = std::fopen(path, "wb");
            if (record_file == nullptr) {
                std::fprintf(stderr, "[TRW] input record: can't write %s\n", path);
            }
        }
    }
}

bool trw::inputrec::replaying() {
    check();
    return play_file != nullptr;
}

bool trw::inputrec::replay(int port, uint16_t* buttons, float* x, float* y, Extras* extras) {
    Read read{};
    if (play_file == nullptr || std::fread(&read, sizeof(read), 1, play_file) != 1) {
        if (!play_ended) {
            play_ended = true;
            std::fprintf(stderr, "[TRW] input replay: end of the recording\n");
        }
        *buttons = 0;
        *x = *y = 0.0f;
        *extras = Extras{};
        return port == 0;
    }
    if (read.port != port) {
        std::fprintf(stderr, "[TRW] input replay: read for port %d, recorded port %d\n", port, read.port);
    }
    *buttons = (uint16_t)read.buttons;
    *x = read.x;
    *y = read.y;
    *extras = read.extras;
    return read.response != 0;
}

void trw::inputrec::record(int port, bool response, uint16_t buttons, float x, float y, const Extras& extras) {
    check();
    if (record_file == nullptr) {
        return;
    }
    Read read{ port, response ? 1 : 0, buttons, x, y, extras };
    std::fwrite(&read, sizeof(read), 1, record_file);
    std::fflush(record_file);
}
