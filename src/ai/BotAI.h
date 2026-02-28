#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  BotAI.h  –  Finite State Machine: Patrol → Chase/Seek → Shoot
//
//  States:
//    PATROL   – walk along waypoint path, vision-check every 100ms
//    ENGAGE   – face target, move to cover, shoot when LOS is clear
//    SEARCH   – move to last known position after losing sight
//    RETREAT  – if HP < 25 and ally alive, fall back to spawn
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/Physics.h"
#include "../weapons/WeaponSystem.h"
#include <raymath.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

enum class BotFSMState : uint8_t {
    PATROL,
    ENGAGE,
    SEARCH,
    RETREAT
};

struct BotBrain {
    BotFSMState state       = BotFSMState::PATROL;
    int         waypointIdx = 0;     // current patrol target
    int         targetID    = -1;    // pawn being engaged
    Vector3     lastKnown   = {};    // last seen enemy position
    float       visionTimer = 0.0f;  // countdown to next raycast check
    float       reactionTimer = 0.0f;// delay before shooting
    float       strafeTimer = 0.0f;  // stagger direction change
    float       strafeSign  = 1.0f;
    bool        hasSightLine= false;
};

// One BotBrain per bot; index matches World::pawns index
static BotBrain s_brains[MAX_PAWNS];

// ─── Find nearest enemy (respects smoke occlusion) ───────────────────────────
static int FindVisibleEnemy(int botID, const World& world) {
    const Pawn& bot = world.pawns[botID];
    Vector3     eye = const_cast<Pawn&>(world.pawns[botID]).eyePos();

    float bestDist = BOT_VISION_RANGE * BOT_VISION_RANGE;
    int   bestID   = -1;

    for(int i = 0; i < MAX_PAWNS; i++) {
        const Pawn& p = world.pawns[i];
        if(!p.alive || p.team == bot.team || i == botID) continue;

        Vector3 toEnemy = Vector3Subtract(
            Vector3Add(p.xform.pos, {0, PLAYER_HEIGHT*0.5f, 0}),
            eye
        );
        float d2 = Vector3LengthSqr(toEnemy);
        if(d2 > bestDist) continue;

        // FOV check
        Vector3 eyeDir = const_cast<Pawn&>(world.pawns[botID]).lookDir();
        float   dot    = Vector3DotProduct(Vector3Normalize(toEnemy), eyeDir);
        if(dot < BOT_VISION_DOT - 0.3f) continue;  // bots have slightly wider awareness

        // Geometry occlusion
        HitResult hr = RaycastSolids(eye, Vector3Normalize(toEnemy),
                                     sqrtf(d2), world.solids);
        if(hr.hit && hr.distance < sqrtf(d2) - 0.2f) continue;

        // Smoke occlusion
        Vector3 enemyPos = Vector3Add(p.xform.pos, {0, PLAYER_HEIGHT*0.5f, 0});
        if(RayBlockedBySmoke(eye, enemyPos, world.smokes)) continue;

        bestDist = d2;
        bestID   = i;
    }
    return bestID;
}

// ─── Move bot towards a world position ────────────────────────────────────────
static void MoveBotToward(Pawn& bot, Vector3 target, float dt,
                          const std::vector<MapSolid>& solids,
                          float strafeSign = 0.0f) {
    Vector3 toTarget = Vector3Subtract(target, bot.xform.pos);
    toTarget.y = 0;
    float dist = Vector3Length(toTarget);
    if(dist < 0.05f) return;

    Vector3 forward = Vector3Scale(Vector3Normalize(toTarget), 1.0f);

    // Optional strafe perpendicular
    Vector3 right   = { forward.z, 0, -forward.x };
    Vector3 move    = Vector3Add(forward, Vector3Scale(right, strafeSign * 0.3f));
    move = Vector3Normalize(move);

    bot.velocity.x = move.x * BOT_SPEED;
    bot.velocity.z = move.z * BOT_SPEED;
    if(!bot.onGround) bot.velocity.y += GRAVITY * dt;
    else              bot.velocity.y  = 0;

    bool onGnd = bot.onGround;
    bot.xform.pos = SweepAABB(bot.xform.pos, bot.velocity, dt, onGnd, solids);
    bot.onGround  = onGnd;

    // Face movement direction
    bot.xform.yaw = atan2f(toTarget.x, toTarget.z);
}

// ─── Aim bot at enemy with noise ──────────────────────────────────────────────
static void AimAtTarget(Pawn& bot, Vector3 targetPos) {
    Vector3 eye   = bot.eyePos();
    Vector3 delta = Vector3Subtract(targetPos, eye);
    float   dist  = Vector3Length(delta);
    if(dist < 0.01f) return;

    // Add per-frame noise
    float noiseX = ((float)rand()/RAND_MAX - 0.5f) * BOT_AIM_NOISE_RAD * 2.0f;
    float noiseY = ((float)rand()/RAND_MAX - 0.5f) * BOT_AIM_NOISE_RAD;
    delta.x += noiseX * dist;
    delta.y += noiseY * dist;

    bot.xform.yaw   = atan2f(delta.x, delta.z);
    bot.xform.pitch = atan2f(delta.y, sqrtf(delta.x*delta.x + delta.z*delta.z));
    bot.xform.pitch = std::clamp(bot.xform.pitch, -1.3f, 1.3f);
}

// ─── Nearest waypoint index ───────────────────────────────────────────────────
static int NearestWaypoint(Vector3 pos, const std::vector<Waypoint>& wps) {
    int   best = 0;
    float bestD = 1e9f;
    for(int i = 0; i < (int)wps.size(); i++) {
        float d = Vector3LengthSqr(Vector3Subtract(pos, wps[i].pos));
        if(d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main per-frame update for all bots
// ─────────────────────────────────────────────────────────────────────────────
inline void UpdateBots(World& world, float dt) {
    for(int i = 0; i < MAX_PAWNS; i++) {
        Pawn& bot = world.pawns[i];
        if(!bot.isBot || !bot.alive) continue;

        BotBrain& brain = s_brains[i];

        // ── Weapon tick ─────────────────────────────────────────────────────
        WeaponTick(bot.weapon, dt);

        // ── Vision raycast (throttled to BOT_RAYCAST_HZ) ──────────────────
        brain.visionTimer -= dt;
        if(brain.visionTimer <= 0) {
            brain.visionTimer = 1.0f / BOT_RAYCAST_HZ;
            int vis = FindVisibleEnemy(i, world);
            if(vis >= 0) {
                brain.targetID  = vis;
                brain.lastKnown = world.pawns[vis].xform.pos;
                brain.hasSightLine = true;
                brain.state = BotFSMState::ENGAGE;
            } else if(brain.targetID >= 0) {
                brain.hasSightLine = false;
                if(brain.state == BotFSMState::ENGAGE)
                    brain.state = BotFSMState::SEARCH;
            }
        }

        // ── Retreat trigger ───────────────────────────────────────────────
        if(bot.hp < 25 && world.aliveCount(bot.team) > 1)
            brain.state = BotFSMState::RETREAT;

        // ── FSM ──────────────────────────────────────────────────────────
        switch(brain.state) {
        // ────────────────────────────────────────────────────────────────
        case BotFSMState::PATROL: {
            if(world.waypoints.empty()) break;
            Waypoint& wp = world.waypoints[brain.waypointIdx % world.waypoints.size()];
            MoveBotToward(bot, wp.pos, dt, world.solids);

            float d = Vector3Length(Vector3Subtract(bot.xform.pos, wp.pos));
            if(d < BOT_WAYPOINT_REACH) {
                // Advance to next waypoint
                if(!wp.neighbours.empty())
                    brain.waypointIdx = wp.neighbours[rand() % wp.neighbours.size()];
                else
                    brain.waypointIdx = (brain.waypointIdx + 1) % world.waypoints.size();
            }
            break;
        }
        // ────────────────────────────────────────────────────────────────
        case BotFSMState::ENGAGE: {
            if(brain.targetID < 0 || !world.pawns[brain.targetID].alive) {
                brain.state    = BotFSMState::PATROL;
                brain.targetID = -1;
                break;
            }
            Pawn& target = world.pawns[brain.targetID];
            Vector3 aimAt = { target.xform.pos.x,
                              target.xform.pos.y + PLAYER_HEIGHT * 0.6f,
                              target.xform.pos.z };
            AimAtTarget(bot, aimAt);

            // Strafe while engaging
            brain.strafeTimer -= dt;
            if(brain.strafeTimer <= 0) {
                brain.strafeTimer = 0.8f + (float)rand()/RAND_MAX * 1.2f;
                brain.strafeSign  = ((rand()%2) ? 1.0f : -1.0f);
            }

            float engageDist = Vector3Length(
                Vector3Subtract(bot.xform.pos, target.xform.pos));

            // Keep distance ~8-15m
            if(engageDist > 15.0f)
                MoveBotToward(bot, target.xform.pos, dt, world.solids, brain.strafeSign);
            else if(engageDist < 6.0f)
                MoveBotToward(bot, Vector3Add(bot.xform.pos,
                    Vector3Scale(Vector3Normalize(
                        Vector3Subtract(bot.xform.pos, target.xform.pos)), 1.0f)),
                    dt, world.solids);
            else {
                // Stand and strafe
                Vector3 right = { bot.lookDir().z, 0, -bot.lookDir().x };
                Vector3 vel = Vector3Scale(right, brain.strafeSign * BOT_SPEED * 0.5f);
                bot.velocity.x = vel.x;
                bot.velocity.z = vel.z;
                if(!bot.onGround) bot.velocity.y += GRAVITY * dt;
                else bot.velocity.y = 0;
                bool og = bot.onGround;
                bot.xform.pos = SweepAABB(bot.xform.pos, bot.velocity, dt, og, world.solids);
                bot.onGround = og;
            }

            // Shoot after reaction delay
            if(brain.hasSightLine) {
                brain.reactionTimer -= dt;
                if(brain.reactionTimer <= 0) {
                    WeaponFire(bot, world, false);
                }
            } else {
                brain.reactionTimer = BOT_REACTION_MS / 1000.0f;
            }
            break;
        }
        // ────────────────────────────────────────────────────────────────
        case BotFSMState::SEARCH: {
            MoveBotToward(bot, brain.lastKnown, dt, world.solids);
            float d = Vector3Length(Vector3Subtract(bot.xform.pos, brain.lastKnown));
            if(d < BOT_WAYPOINT_REACH * 2.0f) {
                brain.state    = BotFSMState::PATROL;
                brain.targetID = -1;
            }
            break;
        }
        // ────────────────────────────────────────────────────────────────
        case BotFSMState::RETREAT: {
            // Move to nearest friendly waypoint (first waypoint on defend side)
            if(world.waypoints.empty()) break;
            int nearest = NearestWaypoint(bot.xform.pos, world.waypoints);
            MoveBotToward(bot, world.waypoints[nearest].pos, dt, world.solids);
            if(bot.hp > 50) brain.state = BotFSMState::PATROL; // recovered enough
            break;
        }
        }
    }
}

// ─── Initialise bot brains at round start ─────────────────────────────────────
inline void InitBotBrains(const World& world) {
    for(int i = 0; i < MAX_PAWNS; i++) {
        s_brains[i] = BotBrain{};
        if(!world.waypoints.empty())
            s_brains[i].waypointIdx = i % (int)world.waypoints.size();
    }
}
