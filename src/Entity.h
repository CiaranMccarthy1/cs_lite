#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Entity.h  –  All game entities as plain structs (no vtable overhead)
// ─────────────────────────────────────────────────────────────────────────────
#include "Constants.h"
#include <vector>
#include <string>

// ─── Forward declarations ─────────────────────────────────────────────────────
struct World;

// ─────────────────────────────────────────────────────────────────────────────
//  Transform
// ─────────────────────────────────────────────────────────────────────────────
struct Transform3D {
    Vector3 pos   = {0,0,0};
    float   yaw   = 0.0f;  // horizontal look (radians)
    float   pitch = 0.0f;  // vertical look
};

// ─────────────────────────────────────────────────────────────────────────────
//  Weapon instance (per-pawn)
// ─────────────────────────────────────────────────────────────────────────────
struct WeaponState {
    WeaponID id          = WeaponID::RIFLE;
    int      ammoMag     = 30;
    int      ammoReserve = 90;
    float    reloadTimer = 0.0f;   // > 0 while reloading
    float    fireCooldown= 0.0f;   // time until next shot allowed
    bool     isADS       = false;

    const WeaponStats& stats() const { return WEAPON_TABLE[(int)id]; }
    bool canFire() const { return fireCooldown <= 0 && reloadTimer <= 0 && ammoMag > 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Pawn  –  shared by player and bots
// ─────────────────────────────────────────────────────────────────────────────
struct Pawn {
    int         id       = -1;
    Team        team     = Team::NONE;
    bool        isBot    = false;
    bool        alive    = true;

    Transform3D xform;
    Vector3     velocity = {0,0,0};
    bool        onGround = false;

    int         hp       = MAX_HP;

    WeaponState weapon;

    // Utility counts (each pawn starts with 1 of each)
    int         fragCount   = 1;
    int         smokeCount  = 1;
    int         stunCount   = 1;

    // AABB for collision / raycasts (half-extents)
    BoundingBox bbox() const {
        float r = PLAYER_RADIUS;
        return {
            { xform.pos.x - r, xform.pos.y,              xform.pos.z - r },
            { xform.pos.x + r, xform.pos.y + PLAYER_HEIGHT, xform.pos.z + r }
        };
    }

    Vector3 eyePos() const {
        return { xform.pos.x, xform.pos.y + PLAYER_HEIGHT * 0.9f, xform.pos.z };
    }

    Vector3 lookDir() const {
        return {
            cosf(xform.pitch) * sinf(xform.yaw),
            sinf(xform.pitch),
            cosf(xform.pitch) * cosf(xform.yaw)
        };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Projectile  (for visual tracer — gameplay uses instant raycast)
// ─────────────────────────────────────────────────────────────────────────────
struct BulletTracer {
    Vector3 origin;
    Vector3 end;
    float   lifeSec = 0.06f;   // fades quickly
    Color   col     = {255,240,180,255};
};

// ─────────────────────────────────────────────────────────────────────────────
//  Grenade (all utility types share this struct)
// ─────────────────────────────────────────────────────────────────────────────
struct GrenadeEntity {
    UtilityID type;
    Vector3   pos;
    Vector3   vel;
    float     fuseTimer;   // seconds until detonation/activation
    bool      detonated  = false;
    float     activeTimer = 0; // for smoke/stun: time remaining after activation
    int       ownerID    = -1;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Smoke zone (post-detonation persistent sphere)
// ─────────────────────────────────────────────────────────────────────────────
struct SmokeZone {
    Vector3 pos;
    float   radius   = SMOKE_RADIUS;
    float   lifeLeft = SMOKE_DURATION_SEC;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Objective zone (attack team must "hold" it for OBJECTIVE_CAPTURE_SEC)
// ─────────────────────────────────────────────────────────────────────────────
struct ObjectiveZone {
    Vector3 pos;
    float   radius      = 3.0f;
    float   captureProgress = 0.0f;  // 0 → OBJECTIVE_CAPTURE_SEC
    bool    captured    = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Stun overlay state (global screen effect)
// ─────────────────────────────────────────────────────────────────────────────
struct StunState {
    float timeLeft = 0.0f;
    float peak     = STUN_DURATION_SEC;

    float alpha() const {
        if(timeLeft <= 0) return 0.0f;
        return (timeLeft / peak);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Map Waypoint (for bot navigation)
// ─────────────────────────────────────────────────────────────────────────────
struct Waypoint {
    Vector3              pos;
    std::vector<int>     neighbours; // indices into World::waypoints
};

// ─────────────────────────────────────────────────────────────────────────────
//  Static geometry (AABB walls / floors used for collision)
// ─────────────────────────────────────────────────────────────────────────────
struct MapSolid {
    BoundingBox  bounds;
    Color        col;
    bool         isFloor = false;
};
