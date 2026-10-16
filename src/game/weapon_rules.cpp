// Loadout Restrictions (Enhancements tab): multiplayer matches limited to
// one kind of weapon for every player and bot, like a gun game: Hammers,
// Bullet, Energy, Explosive or Scorpion only. Melee (the Warhammer, and the
// Raptor Claw and Mite Bite of the characters that use them) is always
// allowed, so nobody is left unarmed, and so are the weapons picked up in a
// level (Cerebral Bore, PFM Mine, Iron Claw, Sentry Turret). Ammo pickups
// become the allowed kind's ammo, model and all (left as they are in Hammers
// only).
//
// What the game does (docs/RAM_NOTES.md has the rest):
// - Player objects: *0x8013EF84 + port * 0x16E8, ports 0-3 the players,
//   4-7 the bots; +0x5D0 nonzero while in the match.
// - +0x770 (halfword) is the weapon wanted, +0x62E the one held. The
//   functions that switch to the wanted weapon (func_0022FD8C, func_0022FEE4,
//   func_002325DC; the bots' AI and the players' wheel both just set +0x770)
//   are hooked to swap a weapon that isn't allowed.
// - Players' loadouts: a (have, wheel slot) byte pair per weapon id from
//   +0x602. Bots don't use it.
// - Ammo: halfwords +0x5F4 bullet, +0x5F6 energy, +0x5F8 explosive. Each
//   weapon's ammo kind is in its definition (*(0x800CB1FC + id * 4), +0x20).
// - The level's objects: a list of types (halfwords from +0x8, the count at
//   +0x4) at *(0x80119DC8 + 0x94); each placed object (a pickup's first byte
//   is 3) keeps its index into it at +0x4, and its model and what picking it
//   up does follow the type there (func_0024E904). Ammo pickups are types
//   0xBC4/0xBC5 energy, 0xBC6/0xBC7 bullet, 0xBC8/0xBC9 explosive (the
//   amounts in the table at 0x800CDED0). The list is rewritten while the
//   level loads, before the pickups are made, so they come out as the new
//   type. The special weapons' pickups (0x84B-0x84F, table at 0x800CDE70)
//   are other objects and are left alone (made into ammo, a level's load
//   could crash); one picked up can't be used under the rule anyway.
#include <algorithm>
#include <atomic>
#include <cstdint>

#include "recomp.h"
#include "trw_config.h"

namespace {
    using trw::config::WeaponRule;

    constexpr int32_t kPlayerPointer = (int32_t)0x8013EF84;
    constexpr uint32_t kPlayerStride = 0x16E8;
    constexpr int kPlayers = 8;
    constexpr uint32_t kPlayerActive = 0x5D0;
    constexpr uint32_t kWantedWeapon = 0x770;
    constexpr uint32_t kLoadout = 0x602;
    constexpr int kWheelSlots = 8;

    constexpr int32_t kTrialType = (int32_t)0x80140215; // byte: 0 multiplayer
    constexpr int32_t kGameMode = (int32_t)0x801407B8;  // 8 the front end, 0-4 boot
    constexpr uint32_t kFrontEnd = 8;

    // Weapon ids.
    enum : int32_t {
        Warhammer = 0, Boomerang, TekCrossbow, Mag60, Shotgun, AssaultRifle, MiniGun, PlasmaRifle,
        FreezeGun, Flaregun, GrenadeLauncher, ScorpionLauncher, NapalmGel, Emaciator, Inflator,
        ChestBurster, RaptorClaw, MiteBite, CerebralBore, PfmMine, IronClaw, SentryTurret,
        WeaponCount,
    };

    // Pickup types in the level's object list.
    constexpr int32_t kWorld = (int32_t)0x80119DC8;
    constexpr uint32_t kWorldTypes = 0x94;
    constexpr uint16_t kAmmoEnergy = 0xBC4;   // and 0xBC5
    constexpr uint16_t kAmmoBullet = 0xBC6;   // and 0xBC7
    constexpr uint16_t kAmmoExplosive = 0xBC8; // and 0xBC9
    constexpr uint16_t kAmmoLast = 0xBC9;

    bool is_melee(int32_t weapon) {
        return weapon == Warhammer || weapon == RaptorClaw || weapon == MiteBite;
    }

    // The weapons picked up in a level (they aren't in a loadout): usable
    // whatever the rule, but never handed out.
    bool is_special(int32_t weapon) {
        return weapon == CerebralBore || weapon == PfmMine || weapon == IronClaw || weapon == SentryTurret;
    }

    WeaponRule rule = WeaponRule::All;

    bool allowed(int32_t weapon) {
        if (is_melee(weapon) || is_special(weapon)) {
            return true;
        }
        switch (rule) {
            case WeaponRule::All:
                return true;
            case WeaponRule::Hammers:
                return false;
            case WeaponRule::Bullet:
                return weapon == Mag60 || weapon == Shotgun || weapon == AssaultRifle || weapon == MiniGun;
            case WeaponRule::Energy:
                return weapon == Boomerang || weapon == TekCrossbow || weapon == PlasmaRifle || weapon == FreezeGun ||
                    weapon == Flaregun || weapon == Emaciator || weapon == Inflator || weapon == ChestBurster;
            case WeaponRule::Explosive:
                return weapon == GrenadeLauncher || weapon == ScorpionLauncher || weapon == NapalmGel;
            case WeaponRule::Scorpion:
                return weapon == ScorpionLauncher;
        }
        return true;
    }

    // A bot (ports 4-7) with ammo for the rule's guns: it gets the stand-in
    // instead of the Warhammer too, as its AI would otherwise keep swinging
    // it (it picks among the weapons it would have had). Out of ammo it may
    // still fight with it. The characters whose melee is the Raptor Claw or
    // the Mite Bite fight only with that, and keep it.
    bool bot_with_ammo(uint8_t* rdram, uint32_t player) {
        uint32_t first = (uint32_t)MEM_W(0, (gpr)kPlayerPointer);
        if (player < first || (player - first) / kPlayerStride < 4) {
            return false;
        }
        uint32_t ammo;
        switch (rule) {
            case WeaponRule::Bullet: ammo = 0x5F4; break;
            case WeaponRule::Energy: ammo = 0x5F6; break;
            case WeaponRule::Explosive:
            case WeaponRule::Scorpion: ammo = 0x5F8; break;
            default: return false; // all or hammers: melee is fine
        }
        return MEM_H(ammo, (gpr)(int32_t)player) > 0;
    }

    // Whether `player` may switch to `weapon`.
    bool allowed_for(uint8_t* rdram, uint32_t player, int32_t weapon) {
        return allowed(weapon) && !(weapon == Warhammer && bot_with_ammo(rdram, player));
    }

    // What a player gets in place of a weapon that isn't allowed.
    int32_t stand_in(uint8_t* rdram, uint32_t player) {
        switch (rule) {
            case WeaponRule::Bullet: return Mag60;
            case WeaponRule::Energy: return TekCrossbow;
            case WeaponRule::Explosive: return GrenadeLauncher;
            case WeaponRule::Scorpion: return ScorpionLauncher;
            default: break;
        }
        // Hammers only: the player's own melee weapon.
        gpr p = (gpr)(int32_t)player;
        for (int32_t melee : { RaptorClaw, MiteBite }) {
            if (MEM_B(kLoadout + melee * 2, p) != 0) {
                return melee;
            }
        }
        return Warhammer;
    }

    bool valid_player(uint32_t player) {
        return player >= 0x80000400u && player < 0x80800000u && (player & 3) == 0;
    }

    // Only the players' loadouts are kept to the allowed weapons: melee and
    // allowed weapons where they were, then the other allowed weapons in the
    // free wheel slots (as many as fit).
    void restrict_loadout(uint8_t* rdram, uint32_t player) {
        gpr p = (gpr)(int32_t)player;
        auto have = [&](int32_t weapon) -> int8_t& { return MEM_B(kLoadout + weapon * 2, p); };
        auto slot = [&](int32_t weapon) -> uint8_t& { return MEM_BU(kLoadout + weapon * 2 + 1, p); };
        bool used[kWheelSlots] = {};
        bool placed[WeaponCount] = {};
        // A special weapon picked up keeps its slot first; specials are left
        // as the game has them otherwise.
        for (int pass = 0; pass < 2; pass++) {
            for (int32_t weapon = 0; weapon < WeaponCount; weapon++) {
                if (is_special(weapon) != (pass == 0)) {
                    continue;
                }
                if (allowed(weapon) && have(weapon) != 0 && slot(weapon) < kWheelSlots && !used[slot(weapon)]) {
                    used[slot(weapon)] = true;
                    placed[weapon] = true;
                }
            }
        }
        int next = 0;
        for (int32_t weapon = 0; weapon < WeaponCount; weapon++) {
            if (placed[weapon] || is_melee(weapon) || is_special(weapon)) {
                continue;
            }
            if (allowed(weapon)) {
                while (next < kWheelSlots && used[next]) {
                    next++;
                }
                if (next < kWheelSlots) {
                    have(weapon) = 1;
                    slot(weapon) = (uint8_t)next;
                    used[next] = true;
                    continue;
                }
            }
            have(weapon) = 0;
            slot(weapon) = 0xFF;
        }
    }
}

namespace {
    // The level's object list, with the ammo pickups made the allowed kind's
    // ammo. Each list is changed once (a new level loads a
    // new one), and only into an ammo type the level has, whose model is loaded.
    uint32_t converted_list = 0;
    // The rule of the match being played (the option as the level loaded).
    WeaponRule match_rule = WeaponRule::All;
    bool rule_latched = false;
    // A multiplayer level is on (the game thread sets it; the UI reads it).
    std::atomic<bool> rule_locked{ false };

    void convert_pickups(uint8_t* rdram) {
        uint32_t list = (uint32_t)MEM_W(kWorldTypes, (gpr)kWorld);
        if (list < 0x80000400u || list >= 0x80800000u || list == converted_list) {
            return;
        }
        gpr l = (gpr)(int32_t)list;
        int32_t count = MEM_W(4, l);
        if (count <= 0 || count > 4096) {
            return;
        }
        uint16_t target = rule == WeaponRule::Bullet ? kAmmoBullet
            : rule == WeaponRule::Energy ? kAmmoEnergy : kAmmoExplosive;
        // Only types the level already has are used: their models are the
        // ones loaded (writing one it lacks crashed the level's load,
        // func_00255E78). Each kind has two (the second a bigger pack,
        // likely); a level may have just one of them.
        bool has[2] = {};
        for (int32_t i = 0; i < count; i++) {
            uint16_t type = MEM_HU(8 + i * 2, l);
            if (type == target || type == target + 1) {
                has[type - target] = true;
            }
        }
        converted_list = list;
        if (!has[0] && !has[1]) {
            return;
        }
        auto available = [&](int second) -> uint16_t {
            return (uint16_t)(target + (has[second] ? second : 1 - second));
        };
        for (int32_t i = 0; i < count; i++) {
            uint16_t type = MEM_HU(8 + i * 2, l);
            if (type >= kAmmoEnergy && type <= kAmmoLast) {
                MEM_HU(8 + i * 2, l) = available((type - kAmmoEnergy) & 1);
            }
        }
    }
}

// Each frame (src/game/extras.cpp, trw_apply_extras).
void trw_apply_weapon_rules(uint8_t* rdram, bool in_match) {
    WeaponRule wanted = trw::config::get_weapon_rule();
    // Multiplayer matches only: the trials have their own weapons.
    bool multiplayer = MEM_BU(0, (gpr)kTrialType) == 0;
    // The pickups are changed as the level loads (before the match is on).
    uint32_t mode = (uint32_t)MEM_W(0, (gpr)kGameMode);
    bool in_level = mode != kFrontEnd && mode > 4;
    if (!in_level) {
        converted_list = 0; // the next level may load its list at the same address
        rule_latched = false;
    }
    else if (!rule_latched) {
        // Chosen as the level loads and kept until the front end: changing
        // the option mid-match does nothing until the next match.
        match_rule = wanted;
        rule_latched = true;
    }
    if (multiplayer && in_level && match_rule != WeaponRule::All && match_rule != WeaponRule::Hammers) {
        rule = match_rule;
        convert_pickups(rdram);
    }
    rule_locked.store(multiplayer && rule_latched);
    rule = (in_match && multiplayer && rule_latched) ? match_rule : WeaponRule::All;
    if (rule == WeaponRule::All) {
        return;
    }
    uint32_t first = (uint32_t)MEM_W(0, (gpr)kPlayerPointer);
    if (!valid_player(first)) {
        return;
    }
    for (int port = 0; port < kPlayers; port++) {
        uint32_t player = first + port * kPlayerStride;
        gpr p = (gpr)(int32_t)player;
        if (MEM_W(kPlayerActive, p) == 0) {
            continue;
        }
        if (port < 4) {
            restrict_loadout(rdram, player);
        }
        int32_t weapon = MEM_H(kWantedWeapon, p);
        if (weapon >= 0 && weapon < WeaponCount && !allowed_for(rdram, player, weapon)) {
            MEM_H(kWantedWeapon, p) = (int16_t)stand_in(rdram, player);
        }
    }
}

// Whether the Loadout Restrictions option is greyed out: from the level loading until
// the front end (any thread).
bool trw::config::weapon_rule_locked() {
    return rule_locked.load();
}

// The weapon-switch functions, with the weapon wanted: the one to switch to.
extern "C" int32_t trw_allowed_weapon(uint8_t* rdram, gpr player, int32_t weapon) {
    if (weapon < 0 || weapon >= WeaponCount || allowed_for(rdram, (uint32_t)player, weapon)) {
        return weapon;
    }
    int32_t replacement = stand_in(rdram, (uint32_t)player);
    MEM_H(kWantedWeapon, player) = (int16_t)replacement;
    return replacement;
}

// A player (re)spawning in a match (func_0021AF6C, which gives 50 bullet
// ammo and no energy or explosive in multiplayer): also some of the allowed
// kind's, about two packs, so nobody starts a restricted match unable to
// shoot. Read off the rule the match started with (the spawns at its start
// come before it counts as in play).
extern "C" void trw_starting_ammo(uint8_t* rdram, gpr player) {
    constexpr uint32_t kEnergyAmmo = 0x5F6, kExplosiveAmmo = 0x5F8;
    constexpr int16_t kEnergyStart = 100, kExplosiveStart = 10;
    if (!rule_latched || MEM_BU(0, (gpr)kTrialType) != 0 || !valid_player((uint32_t)player)) {
        return;
    }
    switch (match_rule) {
        case WeaponRule::Energy:
            MEM_H(kEnergyAmmo, player) = std::max<int16_t>(MEM_H(kEnergyAmmo, player), kEnergyStart);
            break;
        case WeaponRule::Explosive:
        case WeaponRule::Scorpion:
            MEM_H(kExplosiveAmmo, player) = std::max<int16_t>(MEM_H(kExplosiveAmmo, player), kExplosiveStart);
            break;
        default:
            break; // bullet: the game's own 50
    }
}

// The HUD's ammo counters (func_0021EA30; 0 bullet, 1 energy, 2 explosive):
// 1 to draw the counter, 0 to leave out a kind of ammo no allowed weapon uses.
extern "C" int trw_ammo_counter_shown(int kind) {
    switch (rule) {
        case WeaponRule::All: return 1;
        case WeaponRule::Bullet: return kind == 0;
        case WeaponRule::Energy: return kind == 1;
        case WeaponRule::Explosive:
        case WeaponRule::Scorpion: return kind == 2;
        default: return 0; // Hammers only
    }
}
