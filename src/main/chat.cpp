// Netplay chat: a few lines at the left of the screen, like an MMO's chat
// box, and a line to type in. The Chat binding (Controls tab) opens it in a
// netplay match, and in the game's menus by its keys other than Enter (which
// selects there): not in the pause menu or the fly-through (those keys are
// the game's there, as before), nor while the menu (F1) is open. If one of
// those comes up while it's open, it closes, keeping what's typed. While it's open every key goes to it and this PC's
// player gets no input: Enter sends,
// Escape clears what's typed and a second Escape closes it (a third is the
// game's again: the pause menu). On a controller, B is Escape and the Chat
// button again sends. The keys and buttons that open it don't reach the game
// either (chat_held_buttons). Messages are sent through the host
// (src/main/netplay.cpp); each PC allows ten in any 30 seconds.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "SDL.h"
#include "recompinput/input_state.h"
#include "recompinput/input_types.h"
#include "recompinput/players.h"
#include "recompinput/profiles.h"
#include "recompui/recompui.h"
#include "elements/ui_container.h"
#include "elements/ui_label.h"
#include "trw_chat.h"
#include "trw_controls.h"
#include "trw_netplay.h"

namespace {
    using namespace recompui;
    using recompinput::GameInput;
    using recompinput::InputField;
    using clock = std::chrono::steady_clock;

    constexpr size_t kShownLines = 6;   // while not typing, the newest, for kLineSeconds
    constexpr size_t kTypingLines = 10; // while typing
    constexpr auto kLineTime = std::chrono::seconds(12);
    constexpr size_t kRateCount = 10;   // messages in any kRateWindow
    constexpr auto kRateWindow = std::chrono::seconds(30);

    struct Line {
        int port;          // -1: a note from the chat itself
        std::string who;   // "Player 2 (Adon)", as it was when the line came
        std::string text;
        clock::time_point when;
    };

    std::mutex chat_mutex;
    // Where the chat can open (set_allowed): 0 nowhere (paused, the
    // fly-through, the boot screens), 1 the game's menus (not by Enter, which
    // selects there), 2 a match.
    std::atomic<int> allowed{ 0 };
    bool open = false;
    std::string draft;
    SDL_Scancode opened_by = SDL_SCANCODE_UNKNOWN; // its text (a letter key's) isn't typed
    std::deque<Line> lines;
    std::deque<clock::time_point> sent; // this PC's, for the rate limit

    int profile_for(recompinput::InputDevice device) {
        namespace profiles = recompinput::profiles;
        if (recompinput::players::is_single_player_mode()) {
            return device == recompinput::InputDevice::Keyboard ? profiles::get_sp_keyboard_profile_index()
                                                                : profiles::get_sp_controller_profile_index();
        }
        return profiles::get_input_profile_for_player(0, device);
    }

    template <typename Each>
    void each_chat_binding(Each each) {
        for (auto device : { recompinput::InputDevice::Keyboard, recompinput::InputDevice::Controller }) {
            int profile = profile_for(device);
            if (profile < 0) {
                continue;
            }
            for (size_t slot = 0; slot < recompinput::num_bindings_per_input; slot++) {
                const InputField& field = recompinput::profiles::get_input_binding(profile, GameInput::OPEN_CHAT, slot);
                if (!field.is_empty()) {
                    each(profile, field);
                }
            }
        }
    }

    bool is_chat_input(const InputField& pressed) {
        bool found = false;
        each_chat_binding([&](int, const InputField& field) {
            found = found || field == pressed;
        });
        return found;
    }

    void add_line_locked(int port, std::string text) {
        // A note replaces the one before it, if that's the last line.
        if (port < 0 && !lines.empty() && lines.back().port < 0) {
            lines.pop_back();
        }
        std::string who;
        if (port >= 0) {
            who = trw::netplay::player_name(port);
            std::string character = trw::controls::character_name(port);
            if (!character.empty()) {
                who += " (" + character + ")";
            }
        }
        lines.push_back(Line{ port, std::move(who), std::move(text), clock::now() });
        while (lines.size() > 50) {
            lines.pop_front();
        }
    }

    void open_chat(SDL_Scancode by_key, bool by_controller) {
        open = true;
        opened_by = by_key;
        SDL_StartTextInput();
        // Steam's on-screen keyboard (Steam Deck, Big Picture), when Steam
        // started the game.
        if (by_controller && (std::getenv("SteamDeck") != nullptr || std::getenv("SteamGameId") != nullptr)) {
            SDL_OpenURL("steam://open/keyboard");
        }
    }

    void close_chat() {
        open = false;
        opened_by = SDL_SCANCODE_UNKNOWN;
    }

    // Escape: what's typed goes, then the line closes.
    void escape() {
        if (!draft.empty()) {
            draft.clear();
        }
        else {
            close_chat();
        }
    }

    void send() {
        std::string text = trw::chat::sanitize(draft);
        if (text.empty()) {
            draft.clear();
            close_chat();
            return;
        }
        clock::time_point now = clock::now();
        while (!sent.empty() && now - sent.front() >= kRateWindow) {
            sent.pop_front();
        }
        if (sent.size() >= kRateCount) {
            auto wait = std::chrono::duration_cast<std::chrono::seconds>(kRateWindow - (now - sent.front())).count() + 1;
            add_line_locked(-1, "Too many messages. Wait " + std::to_string(wait) + (wait == 1 ? " second." : " seconds."));
            return; // still open, with what was typed
        }
        if (!trw::netplay::send_chat(text)) {
            add_line_locked(-1, "Nobody to send it to.");
        }
        else {
            sent.push_back(now);
            add_line_locked(trw::netplay::local_port(), text);
        }
        draft.clear();
        close_chat();
    }

    void type(std::string_view text) {
        for (char c : text) {
            if (draft.size() >= trw::chat::kMaxLength) {
                break;
            }
            if (c >= 0x20 && c <= 0x7E) {
                draft.push_back(c);
            }
        }
    }

    // ------------------------------------------- the box over the game

    ContextId chat_context = ContextId::null();
    Element* chat_box = nullptr;
    std::vector<Label*> line_labels;
    Label* input_label = nullptr;
    std::string shown; // what the box shows now, to skip updates that change nothing

    void make_chat_overlay() {
        chat_context = create_context();
        chat_context.open();
        chat_context.set_captures_input(false);
        chat_context.set_captures_mouse(false);
        Element* root = chat_context.get_root_element();
        root->set_width(100.0f, Unit::Percent);
        root->set_height(100.0f, Unit::Percent);
        chat_box = chat_context.create_element<Container>(root, FlexDirection::Column, JustifyContent::FlexEnd);
        chat_box->set_position(Position::Absolute);
        chat_box->set_left(1.5f, Unit::Percent);
        chat_box->set_bottom(24.0f, Unit::Percent);
        chat_box->set_width(34.0f, Unit::Percent);
        chat_box->set_padding(8.0f);
        chat_box->set_gap(2.0f);
        chat_box->set_border_radius(theme::border::radius_sm);
        chat_box->set_display(Display::None);
        for (size_t i = 0; i < kTypingLines; i++) {
            Label* label = chat_context.create_element<Label>(chat_box, "", theme::Typography::LabelSM);
            label->set_white_space(WhiteSpace::Normal);
            label->set_display(Display::None);
            line_labels.push_back(label);
        }
        input_label = chat_context.create_element<Label>(chat_box, "", theme::Typography::LabelSM);
        input_label->set_white_space(WhiteSpace::Normal);
        input_label->set_color(theme::color::Primary);
        input_label->set_display(Display::None);
        chat_context.close();
        show_context(chat_context, "");
    }

    std::string line_text(const Line& line) {
        if (line.port < 0) {
            return line.text;
        }
        return line.who + ": " + line.text;
    }
}

std::string trw::chat::sanitize(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (c >= 0x20 && c <= 0x7E && out.size() < kMaxLength) {
            out.push_back(c);
        }
    }
    size_t start = out.find_first_not_of(' ');
    if (start == std::string::npos) {
        return "";
    }
    size_t end = out.find_last_not_of(' ');
    return out.substr(start, end - start + 1);
}

void trw::chat::set_defaults() {
    recompinput::set_game_input_description(GameInput::OPEN_CHAT,
        "Netplay: opens the chat line. Enter sends, Escape clears it and then closes it.");
    // Enter and T; none on a controller (no keyboard to type with there,
    // and its buttons are all the game's), though one can be bound.
    recompinput::set_default_mapping_for_keyboard(GameInput::OPEN_CHAT,
        { InputField::keyboard(SDL_SCANCODE_RETURN), InputField::keyboard(SDL_SCANCODE_T) });
}

// Once (a marker file beside the controls config): profiles saved with the
// first defaults (Enter and Y; Y on a controller) get the new ones. Ones
// changed by hand stay.
void trw::chat::update_profiles(const std::filesystem::path& config_dir) {
    std::filesystem::path marker = config_dir / "chat_keys_v2";
    std::error_code ec;
    if (std::filesystem::exists(marker, ec)) {
        return;
    }
    namespace profiles = recompinput::profiles;
    for (int profile = 0; profile < profiles::get_input_profile_count(); profile++) {
        recompinput::InputDevice device = profiles::get_input_profile_device(profile);
        const InputField first = profiles::get_input_binding(profile, GameInput::OPEN_CHAT, 0);
        const InputField second = profiles::get_input_binding(profile, GameInput::OPEN_CHAT, 1);
        bool old_keys = device == recompinput::InputDevice::Keyboard &&
            first == InputField::keyboard(SDL_SCANCODE_RETURN) && second == InputField::keyboard(SDL_SCANCODE_Y);
        bool old_button = device == recompinput::InputDevice::Controller &&
            first == InputField::controller_digital(SDL_CONTROLLER_BUTTON_Y) && second.is_empty();
        if (old_keys || old_button) {
            profiles::reset_input_binding(profile, device, GameInput::OPEN_CHAT);
        }
    }
    profiles::save_controls_config(config_dir / "controls.json");
    FILE* f = std::fopen(marker.string().c_str(), "w");
    if (f != nullptr) {
        std::fputs("Chat keys moved to Enter and T, none on controllers.\n", f);
        std::fclose(f);
    }
}

bool trw::chat::handle_event(const SDL_Event& event) {
    std::lock_guard lock{ chat_mutex };
    if (!trw::netplay::active()) {
        if (open) {
            close_chat();
        }
        return false;
    }
    // Out of a match (its menus, the pause menu): the keys are the game's.
    // A line still open closes, and the key that found it so goes nowhere.
    if (allowed.load() == 0) {
        if (!open) {
            return false;
        }
        close_chat();
        return event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT || event.type == SDL_CONTROLLERBUTTONDOWN;
    }
    switch (event.type) {
    case SDL_KEYDOWN: {
        SDL_Scancode sc = event.key.keysym.scancode;
        if (open) {
            // The menu (F1) opened over it: the menu has the keys.
            if (recompinput::game_input_disabled()) {
                close_chat();
                return false;
            }
            if (sc == SDL_SCANCODE_ESCAPE) {
                if (event.key.repeat == 0) {
                    escape();
                }
            }
            else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
                if (event.key.repeat == 0) {
                    send();
                }
            }
            else if (sc == SDL_SCANCODE_BACKSPACE) {
                if (!draft.empty()) {
                    draft.pop_back();
                }
            }
            else if (sc == SDL_SCANCODE_V && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                char* clip = SDL_GetClipboardText();
                if (clip != nullptr) {
                    std::string text = clip;
                    SDL_free(clip);
                    std::replace(text.begin(), text.end(), '\n', ' ');
                    std::replace(text.begin(), text.end(), '\t', ' ');
                    type(text);
                }
            }
            return true;
        }
        bool enter = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
        bool here = allowed.load() == 2 || !enter; // Enter selects in the menus
        if (here && event.key.repeat == 0 && !recompinput::game_input_disabled() && is_chat_input(InputField::keyboard(sc))) {
            open_chat(sc, false);
            return true;
        }
        return false;
    }
    case SDL_KEYUP:
        if (open && event.key.keysym.scancode == opened_by) {
            opened_by = SDL_SCANCODE_UNKNOWN;
        }
        return open;
    case SDL_TEXTINPUT:
        if (!open) {
            return false;
        }
        // The letter of the key that opened it (T), on its way still.
        if (opened_by != SDL_SCANCODE_UNKNOWN && draft.empty() && event.text.text[0] != '\0' && event.text.text[1] == '\0') {
            opened_by = SDL_SCANCODE_UNKNOWN;
            return true;
        }
        type(event.text.text);
        return true;
    case SDL_CONTROLLERBUTTONDOWN: {
        InputField pressed = InputField::controller_digital((SDL_GameControllerButton)event.cbutton.button);
        if (open) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                escape();
            }
            else if (is_chat_input(pressed)) {
                send();
            }
            return true;
        }
        if (!recompinput::game_input_disabled() && is_chat_input(pressed)) {
            open_chat(SDL_SCANCODE_UNKNOWN, true);
            return true;
        }
        return false;
    }
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEWHEEL:
        return open;
    default:
        return false;
    }
}

void trw::chat::set_allowed(Where where) {
    allowed.store(static_cast<int>(where));
}

bool trw::chat::typing() {
    std::lock_guard lock{ chat_mutex };
    return open;
}

uint16_t trw::chat::chat_held_buttons() {
#define DEFINE_INPUT(name, value, readable) { GameInput::name, (uint16_t)value },
    static const std::pair<GameInput, uint16_t> buttons[] = { DEFINE_N64_BUTTON_INPUTS() };
#undef DEFINE_INPUT
    uint16_t mask = 0;
    each_chat_binding([&](int profile, const InputField& field) {
        if (!recompinput::get_input_digital(0, field)) {
            return;
        }
        for (const auto& [input, value] : buttons) {
            for (size_t slot = 0; slot < recompinput::num_bindings_per_input; slot++) {
                if (recompinput::profiles::get_input_binding(profile, input, slot) == field) {
                    mask |= value;
                }
            }
        }
    });
    return mask;
}

void trw::chat::add_line(int port, std::string_view text) {
    std::string clean = sanitize(text);
    if (clean.empty()) {
        return;
    }
    std::lock_guard lock{ chat_mutex };
    add_line_locked(port, std::move(clean));
}

void trw::chat::tick() {
    std::vector<std::string> show;
    std::string input;
    bool typing_now;
    {
        std::lock_guard lock{ chat_mutex };
        if (open && (recompinput::game_input_disabled() || allowed.load() == 0)) {
            close_chat(); // the menu (F1) opened over it, or the match is over or paused
        }
        typing_now = open;
        clock::time_point now = clock::now();
        size_t most = typing_now ? kTypingLines : kShownLines;
        for (auto it = lines.rbegin(); it != lines.rend() && show.size() < most; ++it) {
            if (!typing_now && now - it->when >= kLineTime) {
                break;
            }
            show.push_back(line_text(*it));
        }
        std::reverse(show.begin(), show.end());
        if (typing_now) {
            input = "Say: " + draft + "_";
            if (draft.size() >= kMaxLength - 30) {
                input += "  (" + std::to_string(draft.size()) + "/" + std::to_string(kMaxLength) + ")";
            }
        }
    }

    std::string state = typing_now ? "T" : "-";
    state += input;
    for (const std::string& line : show) {
        state += '\n' + line;
    }
    if (state == shown) {
        return;
    }
    if (chat_context == ContextId::null()) {
        if (show.empty() && !typing_now) {
            return;
        }
        make_chat_overlay();
    }
    // Hidden only when the UI shuts down (it hides every context first).
    if (!is_context_shown(chat_context)) {
        return;
    }
    shown = state;
    ContextId previous = try_close_current_context();
    chat_context.open();
    if (show.empty() && !typing_now) {
        chat_box->set_display(Display::None);
    }
    else {
        chat_box->set_display(Display::Flex);
        chat_box->set_background_color(theme::color::ModalOverlay, typing_now ? 200 : 110);
        for (size_t i = 0; i < line_labels.size(); i++) {
            if (i < show.size()) {
                line_labels[i]->set_text(show[i]);
                line_labels[i]->set_display(Display::Block);
            }
            else {
                line_labels[i]->set_display(Display::None);
            }
        }
        if (typing_now) {
            input_label->set_text(input);
            input_label->set_display(Display::Block);
        }
        else {
            input_label->set_display(Display::None);
        }
    }
    chat_context.close();
    if (previous != ContextId::null()) {
        previous.open();
    }
}
