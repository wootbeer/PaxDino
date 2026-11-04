#ifndef __TRW_UPDATE_H__
#define __TRW_UPDATE_H__

#include <string>

// Is a newer PaxDino out? (src/main/update_check.cpp): one anonymous request
// to GitHub for the latest release, off the UI's thread, at most once a
// launch. Nothing is downloaded or installed.
namespace trw::update {
    constexpr const char* kReleasesUrl = "https://github.com/wootbeer/PaxDino/releases";

    // This build's version ("0.1.7"), at startup.
    void set_current(const std::string& version);
    const std::string& current();
    // Starts the check (once; later calls do nothing).
    void check();
    // The newer release's version ("0.1.8"), once the check has found one;
    // "" while it runs, if this build is the latest, or if GitHub couldn't
    // be reached.
    std::string newer();
}

#endif
