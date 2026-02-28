#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  WeaponSystem.h  –  Hitscan shooting, recoil, reload
// ─────────────────────────────────────────────────────────────────────────────
#include "AudioSystem.h"
#include "../World.h"
#include "../game/Physics.h"

// Forward declare so audio param can default to nullptr without full include
struct AudioSystem;
#include <raymath.h>
#include <cmath>
#include <cstdlib>

// ─── Randomised spread direction ─────────────────────────────────────────────
inline Vector3 ApplySpread(Vector3 dir, float spreadRad) {
    if(spreadRad <= 0.0f) return dir;
    // Random cone using two angles
    float theta = ((float)rand() / RAND_MAX) * 2.0f * PI;
    float phi   = ((float)rand() / RAND_MAX) * spreadRad;
    // Build perpendicular vectors
    Vector3 up    = {0,1,0};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, up));
    if(Vector3Length(right) < 0.01f) { right = {1,0,0}; }
    Vector3 up2   = Vector3CrossProduct(right, dir);
    Vector3 offset = Vector3Add(
        Vector3Scale(right, cosf(theta) * sinf(phi)),
        Vector3Scale(up2,   sinf(theta) * sinf(phi))
    );
    return Vector3Normalize(Vector3Add(dir, offset));
}

// ─── Single shot / pellet trace ───────────────────────────────────────────────
struct ShotResult {
    bool    hitPawn   = false;
    int     hitPawnID = -1;
    int     damage    = 0;
    Vector3 endPoint  = {};
};

inline ShotResult FireRay(
    Vector3              origin,
    Vector3              direction,
    float                maxRange,
    int                  shooterID,
    World&               world)
{
    ShotResult result;
    Ray ray = { origin, direction };

    // 1. Find nearest geometry hit
    HitResult geom = RaycastSolids(origin, direction, maxRange, world.solids);
    float geomDist = geom.hit ? geom.distance : maxRange;

    // 2. Check each pawn's AABB
    float  bestDist = geomDist;
    int    bestID   = -1;

    for(int i = 0; i < MAX_PAWNS; i++) {
        auto& p = world.pawns[i];
        if(!p.alive || i == shooterID) continue;
        // Don't shoot through smoke for bots (player shooting through smoke is allowed)
        RayCollision rc = GetRayCollisionBox(ray, p.bbox());
        if(rc.hit && rc.distance > 0 && rc.distance < bestDist) {
            bestDist = rc.distance;
            bestID   = i;
        }
    }

    if(bestID >= 0) {
        result.hitPawn   = true;
        result.hitPawnID = bestID;
        result.endPoint  = Vector3Add(origin, Vector3Scale(direction, bestDist));
    } else {
        result.endPoint  = geom.hit ? geom.point
                         : Vector3Add(origin, Vector3Scale(direction, maxRange));
    }
    return result;
}

// ─── Full weapon fire (handles pellets, spread, cooldown, ammo) ───────────────
inline void WeaponFire(Pawn& shooter, World& world, bool isADS, AudioSystem* audio = nullptr) {
    WeaponState& ws  = shooter.weapon;
    if(!ws.canFire()) return;

    if (audio) audio->PlayShoot(ws.id);

    const WeaponStats& st = ws.stats();
    ws.ammoMag--;
    ws.fireCooldown = 60.0f / st.fireRateRPM;

    float spread = st.spreadRad * (isADS ? st.adsSpreadMult : 1.0f);
    Vector3 eye  = shooter.eyePos();
    Vector3 look = shooter.lookDir();

    for(int p = 0; p < st.pellets; p++) {
        Vector3 dir = ApplySpread(look, spread);
        ShotResult sr = FireRay(eye, dir, st.range, shooter.id, world);

        // Register hit
        if(sr.hitPawn) {
            Pawn& target = world.pawns[sr.hitPawnID];
            target.hp = std::max(0, target.hp - st.damage);
            if(target.hp <= 0) target.alive = false;

            // Hit flash for player
            if(sr.hitPawnID == world.playerID)
                world.hitIndicatorAlpha = 1.0f;
        }

        // Bullet tracer
        if((int)world.tracers.size() < MAX_TRACERS) {
            Color tc = (shooter.id == world.playerID)
                ? Color{255,240,160,220}
                : Color{255,140,100,200};
            world.tracers.push_back({ eye, sr.endPoint, 0.06f, tc });
        }
    }

    // Auto-reload on empty
    if(ws.ammoMag == 0 && ws.ammoReserve > 0) {
        ws.reloadTimer = st.reloadTimeSec;
    }
}

// ─── Per-frame weapon tick ─────────────────────────────────────────────────────
inline void WeaponTick(WeaponState& ws, float dt) {
    if(ws.fireCooldown  > 0) ws.fireCooldown  -= dt;
    if(ws.reloadTimer   > 0) {
        ws.reloadTimer -= dt;
        if(ws.reloadTimer <= 0) {
            const auto& st = ws.stats();
            int need = st.magSize - ws.ammoMag;
            int take = std::min(need, ws.ammoReserve);
            ws.ammoMag     += take;
            ws.ammoReserve -= take;
        }
    }
}

// ─── Throw utility ────────────────────────────────────────────────────────────
inline bool ThrowUtility(Pawn& thrower, UtilityID type, World& world) {
    int& count = (type == UtilityID::FRAG) ? thrower.fragCount
               : (type == UtilityID::SMOKE) ? thrower.smokeCount
               : thrower.stunCount;
    if(count <= 0) return false;
    count--;

    float fuse = (type == UtilityID::FRAG) ? FRAG_FUSE_SEC : 0.8f;
    Vector3 vel = Vector3Scale(thrower.lookDir(), 12.0f);
    vel.y += 4.0f;  // arc upward

    world.grenades.push_back({ type, thrower.eyePos(), vel, fuse, false, 0.0f, thrower.id });
    return true;
}