#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  World.h  –  All mutable simulation state in one flat struct.
//  No heap allocations in the hot path; sizes are bounded at compile-time.
// ─────────────────────────────────────────────────────────────────────────────
#include "Entity.h"
#include <array>
#include <vector>

// Maximum entities – keeps memory layout predictable
constexpr int MAX_PAWNS     = 6;   // 3v3
constexpr int MAX_GRENADES  = 16;
constexpr int MAX_SMOKES    = 8;
constexpr int MAX_TRACERS   = 64;
constexpr int MAX_SOLIDS    = 256;
constexpr int MAX_WAYPOINTS = 64;

enum class RoundState : uint8_t {
    WAITING,     // pre-round freeze
    ACTIVE,
    ROUND_OVER,
    MATCH_OVER
};

struct World {
    // ── Pawns ────────────────────────────────────────────────────────────────
    std::array<Pawn, MAX_PAWNS>          pawns;
    int                                  playerID = 0;  // index of human pawn

    // ── Map geometry ─────────────────────────────────────────────────────────
    std::vector<MapSolid>                solids;        // AABB list
    std::vector<Waypoint>                waypoints;
    ObjectiveZone                        objective;

    // ── Dynamic entities ─────────────────────────────────────────────────────
    std::vector<GrenadeEntity>           grenades;
    std::vector<SmokeZone>               smokes;
    std::vector<BulletTracer>            tracers;

    // ── Screen effects ────────────────────────────────────────────────────────
    StunState                            stun;
    float                                hitIndicatorAlpha = 0.0f; // red flash on hit

    // ── Round management ─────────────────────────────────────────────────────
    RoundState  roundState  = RoundState::WAITING;
    float       roundTimer  = ROUND_TIME_SEC;
    float       freezeTimer = 3.0f;   // pre-round freeze
    Team        roundWinner = Team::NONE;
    int         scoreAttack = 0;
    int         scoreDefend = 0;
    int         roundNumber = 1;

    // ── Helpers ───────────────────────────────────────────────────────────────
    Pawn& player() { return pawns[playerID]; }
    const Pawn& player() const { return pawns[playerID]; }

    bool alivePawnsOnTeam(Team t) const {
        for(auto& p : pawns) if(p.team == t && p.alive) return true;
        return false;
    }

    int aliveCount(Team t) const {
        int n = 0;
        for(auto& p : pawns) if(p.team == t && p.alive) n++;
        return n;
    }
};
