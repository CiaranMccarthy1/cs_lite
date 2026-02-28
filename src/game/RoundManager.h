#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  RoundManager.h  –  Round lifecycle: spawn, timer, win-condition, reset
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/MapLoader.h"
#include "../ai/BotAI.h"

// ─── Initialise all 6 pawns from spawn data ───────────────────────────────────
inline void SpawnPawns(World& world, const MapData& md) {
    int attackIdx = 0, defendIdx = 0;

    // Collect spawn points per team
    std::vector<SpawnPoint> atkSpawns, defSpawns;
    for(auto& sp : md.spawns) {
        if(sp.team == Team::ATTACK) atkSpawns.push_back(sp);
        else                        defSpawns.push_back(sp);
    }

    for(int i = 0; i < MAX_PAWNS; i++) {
        Pawn& p      = world.pawns[i];
        p.id         = i;
        p.alive      = true;
        p.hp         = MAX_HP;
        p.fragCount  = 1;
        p.smokeCount = 1;
        p.stunCount  = 1;
        p.velocity   = {0,0,0};
        p.onGround   = true;

        // Player is pawn 0 on attack team
        p.isBot = (i != world.playerID);
        p.team  = (i < TEAM_SIZE) ? Team::ATTACK : Team::DEFEND;

        // Default weapon
        p.weapon     = WeaponState{};
        p.weapon.id  = (p.team == Team::ATTACK) ? WeaponID::RIFLE : WeaponID::SMG;
        auto& st     = p.weapon.stats();
        p.weapon.ammoMag     = st.magSize;
        p.weapon.ammoReserve = st.magSize * 3;

        // Position from spawn list
        std::vector<SpawnPoint>& spList = (p.team == Team::ATTACK) ? atkSpawns : defSpawns;
        int& idx = (p.team == Team::ATTACK) ? attackIdx : defendIdx;
        if(!spList.empty()) {
            auto& sp     = spList[idx % spList.size()];
            p.xform.pos  = { sp.pos.x, sp.pos.y + 0.01f, sp.pos.z };
            p.xform.yaw  = sp.yaw;
            p.xform.pitch= 0;
            idx++;
        }
    }
}

// ─── Full round reset ─────────────────────────────────────────────────────────
inline void ResetRound(World& world, const MapData& md) {
    world.grenades.clear();
    world.smokes.clear();
    world.tracers.clear();
    world.stun.timeLeft = 0;
    world.hitIndicatorAlpha = 0;
    world.objective.captureProgress = 0;
    world.objective.captured = false;
    world.roundTimer  = ROUND_TIME_SEC;
    world.roundState  = RoundState::WAITING;
    world.freezeTimer = 3.0f;
    world.roundWinner = Team::NONE;
    SpawnPawns(world, md);
    InitBotBrains(world);
}

// ─── Per-frame round logic ────────────────────────────────────────────────────
inline void UpdateRound(World& world, const MapData& md, float dt) {
    switch(world.roundState) {

    case RoundState::WAITING:
        world.freezeTimer -= dt;
        if(world.freezeTimer <= 0) world.roundState = RoundState::ACTIVE;
        break;

    case RoundState::ACTIVE: {
        world.roundTimer -= dt;

        // ── Objective capture ─────────────────────────────────────────────
        bool anyAttackerInZone = false;
        for(auto& p : world.pawns) {
            if(!p.alive || p.team != Team::ATTACK) continue;
            float d = Vector3Length(Vector3Subtract(p.xform.pos, world.objective.pos));
            if(d < world.objective.radius) { anyAttackerInZone = true; break; }
        }
        if(anyAttackerInZone) {
            world.objective.captureProgress += dt;
            if(world.objective.captureProgress >= OBJECTIVE_CAPTURE_SEC) {
                world.objective.captured = true;
                world.roundWinner = Team::ATTACK;
                world.roundState  = RoundState::ROUND_OVER;
                world.scoreAttack++;
                return;
            }
        } else {
            world.objective.captureProgress = std::max(0.0f,
                world.objective.captureProgress - dt * 0.5f);
        }

        // ── Win conditions ────────────────────────────────────────────────
        bool attackAlive = world.alivePawnsOnTeam(Team::ATTACK);
        bool defendAlive = world.alivePawnsOnTeam(Team::DEFEND);

        if(!attackAlive || world.roundTimer <= 0) {
            world.roundWinner = Team::DEFEND;
            world.roundState  = RoundState::ROUND_OVER;
            world.scoreDefend++;
        } else if(!defendAlive) {
            world.roundWinner = Team::ATTACK;
            world.roundState  = RoundState::ROUND_OVER;
            world.scoreAttack++;
        }
        break;
    }

    case RoundState::ROUND_OVER: {
        // Auto-advance after 4 seconds
        static float overTimer = 4.0f;
        overTimer -= dt;
        if(overTimer <= 0) {
            overTimer = 4.0f;
            world.roundNumber++;
            if(world.scoreAttack >= 5 || world.scoreDefend >= 5) {
                world.roundState = RoundState::MATCH_OVER;
            } else {
                ResetRound(world, md);
            }
        }
        break;
    }

    case RoundState::MATCH_OVER:
        // Wait for key press to restart match
        if(IsKeyPressed(KEY_ENTER)) {
            world.scoreAttack = 0;
            world.scoreDefend = 0;
            world.roundNumber = 1;
            ResetRound(world, md);
        }
        break;
    }
}
