#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  WeaponSystem.h  –  Hitscan shooting, recoil, reload
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/Physics.h"
#include "../audio/AudioSystem.h"

struct AudioSystem;
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <raymath.h>

// ─── Recoil & Spread ─────────────────────────────────────────────────────────
inline Vector3 ApplySpread(Vector3 dir, float inaccuracyRad,
    int shotsFired, WeaponID weaponId, float speed) {
    const bool isShotgun = (weaponId == WeaponID::SHOTGUN);
    const bool lowSpeed = (speed < 0.25f);

    if (isShotgun) {
        if (inaccuracyRad <= 0.0f) return dir;
        Vector3 up = { 0, 1, 0 };
        Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, up));
        if (Vector3Length(right) < 0.01f) right = { 1, 0, 0 };
        Vector3 up2 = Vector3CrossProduct(right, dir);

        float theta = ((float)rand() / RAND_MAX) * 2.0f * PI;
        float phi = ((float)rand() / RAND_MAX) * inaccuracyRad;
        Vector3 offset = Vector3Add(Vector3Scale(right, cosf(theta) * sinf(phi)),
            Vector3Scale(up2, sinf(theta) * sinf(phi)));
        return Vector3Normalize(Vector3Add(dir, offset));
    }

    // First shot perfectly accurate at low speed
    if (shotsFired == 0 && !isShotgun) {
        if (lowSpeed) {
            return dir; // Absolute dead center at low speed
        }
        inaccuracyRad *= 0.1f; // Heavy dampening if slightly spread by move
    }

    // Deterministic Recoil Pattern (pulls up and to the right/left over time)
    // CS:GO style deterministic pattern arrays mapped across steps
    int step = std::min(shotsFired, 30);

    float patternPitch = 0.0f;
    float patternYaw = 0.0f;

    switch (weaponId) {
    case WeaponID::PISTOL:
        patternPitch = step * 0.005f;
        patternYaw = (step % 2 == 0 ? 1 : -1) * 0.002f; // Slight alternating zigzag
        break;
    case WeaponID::SMG:
        // Straight up for 6 shots, hard right for 6 shots, left for the rest
        if (step < 6) {
            patternPitch = step * 0.012f;
            patternYaw = step * 0.002f;
        }
        else if (step < 12) {
            patternPitch = 6 * 0.012f + (step - 6) * 0.004f;
            patternYaw = 6 * 0.002f + (step - 6) * 0.015f; 
        } 
        else {
            patternPitch = 6 * 0.012f + 6 * 0.004f + (step - 12) * 0.001f;
            patternYaw = (6 * 0.002f + 6 * 0.015f) - (step - 12) * 0.012f;
        }
        break;
    case WeaponID::RIFLE:
        // Famous CS:GO AK47/M4A4 "T" pattern
        // Fast vertical climb
        if (step < 8) {
            patternPitch = step * 0.015f;
            patternYaw = step * 0.002f; 
        }
        // Pull right
        else if (step < 16) {
            patternPitch = 8 * 0.015f + (step - 8) * 0.002f; // Plateaus vertically
            patternYaw = 8 * 0.002f + (step - 8) * 0.012f;
        }
        // Pull left hard
        else if (step < 24) {
            patternPitch = 8 * 0.015f + 8 * 0.002f;
            patternYaw = (8 * 0.002f + 8 * 0.012f) - (step - 16) * 0.018f;
        }
        // Hook back to right slightly
        else {
            patternPitch = 8 * 0.015f + 8 * 0.002f;
            float leftPeak = (8 * 0.002f + 8 * 0.012f) - 8 * 0.018f;
            patternYaw = leftPeak + (step - 24) * 0.010f;
        }
        break;
    case WeaponID::SNIPER:
        patternPitch = step * 0.080f; // Massive isolated kick
        patternYaw = 0.0f;
        break;
    case WeaponID::SHOTGUN:
    default:
        patternPitch = step * 0.008f;
        patternYaw = sinf(step * 0.5f) * 0.008f;
        break;
    }

    // Build perpendicular vectors
    Vector3 up = { 0, 1, 0 };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, up));
    if (Vector3Length(right) < 0.01f) right = { 1, 0, 0 };
    Vector3 up2 = Vector3CrossProduct(right, dir);

    Vector3 patternDir = Vector3Normalize(
        Vector3Add(dir,
            Vector3Add(Vector3Scale(up2, patternPitch),
                Vector3Scale(right, patternYaw))));

    // Fully deterministic recoil while moving very slowly
    if (lowSpeed) {
        return patternDir;
    }

    // Above low-speed threshold: add tiny randomness around the deterministic pattern
    float tinySpread = inaccuracyRad * 0.08f;
    if (tinySpread <= 0.0f) {
        return patternDir;
    }

        float theta = ((float)rand() / RAND_MAX) * 2.0f * PI;
        float phi = ((float)rand() / RAND_MAX) * tinySpread;

        Vector3 randOffset = Vector3Add(
            Vector3Scale(right, cosf(theta) * sinf(phi)),
            Vector3Scale(up2, sinf(theta) * sinf(phi)));

        return Vector3Normalize(Vector3Add(patternDir, randOffset));
}

inline float ComputeShotInaccuracy(const Pawn& shooter, bool isADS) {
    const WeaponState& ws = shooter.weapon;
    const WeaponStats& st = ws.stats();

    float inaccuracy = st.spreadRad;
    if (ws.id == WeaponID::SNIPER && !isADS) {
        inaccuracy += 0.45f; // Severe no-scope penalty
    }
    else if (isADS) {
        inaccuracy *= st.adsSpreadMult;
    }

    float speed = sqrtf(shooter.velocity.x * shooter.velocity.x +
        shooter.velocity.z * shooter.velocity.z);

    if (!shooter.onGround) {
        inaccuracy += 0.50f;
    }
    else if (speed > 1.0f) {
        inaccuracy += (speed / PLAYER_SPEED) * 0.22f;
    }
    else if (shooter.isCrouching) {
        inaccuracy *= 0.2f;
    }

    // Sustained sprays widen the pattern envelope.
    inaccuracy += std::min(ws.shotsFired, 16) * 0.0025f;
    return inaccuracy;
}

// ─── Single shot / pellet trace ───────────────────────────────────────────────
struct ShotResult {
    bool    hitPawn = false;
    int     hitPawnID = -1;
    int     damage = 0;
    Vector3 endPoint = {};
    bool    hitGeom = false;
};

inline ShotResult FireRay(Vector3 origin, Vector3 direction, float maxRange,
    int shooterID, Team shooterTeam, World& world) {
    ShotResult result;
    Ray ray = { origin, direction };

    // 1. Find nearest geometry hit
    HitResult geom = RaycastSolids(origin, direction, maxRange, world.solids);
    float     geomDist = geom.hit ? geom.distance : maxRange;

    // 2. Check each pawn's AABB
    float bestDist = geomDist;
    int   bestID = -1;

    for (int i = 0; i < MAX_PAWNS; i++) {
        auto& p = world.pawns[i];
        if (!p.alive || i == shooterID) continue;
        if (!FRIENDLY_FIRE && p.team == shooterTeam) continue;
        RayCollision rc = GetRayCollisionBox(ray, p.bbox());
        if (rc.hit && rc.distance > 0 && rc.distance < bestDist) {
            bestDist = rc.distance;
            bestID = i;
        }
    }

    if (bestID >= 0) {
        result.hitPawn = true;
        result.hitPawnID = bestID;
        result.endPoint = Vector3Add(origin, Vector3Scale(direction, bestDist));
    }
    else {
        result.hitGeom = geom.hit;
        result.endPoint = geom.hit ? geom.point
            : Vector3Add(origin, Vector3Scale(direction, maxRange));
    }
    return result;
}

// ─── Full weapon fire (handles pellets, spread, cooldown, ammo) ───────────────
inline void WeaponFire(Pawn& shooter, World& world, bool isADS,
    AudioSystem* audio = nullptr) {
    WeaponState& ws = shooter.weapon;
    if (!ws.canFire()) return;

    if (audio) audio->PlayShoot(ws.id);

    const WeaponStats& st = ws.stats();
    ws.ammoMag--;
    ws.fireCooldown = 60.0f / st.fireRateRPM;

    // Track consecutive shots for recoil
    ws.shotsFired++;
    ws.timeSinceLastShot = 0.0f;

    float speed = sqrtf(shooter.velocity.x * shooter.velocity.x +
        shooter.velocity.z * shooter.velocity.z);
    float inaccuracy = ComputeShotInaccuracy(shooter, isADS);

    Vector3 eye = shooter.eyePos();
    Vector3 look = shooter.lookDir();

    for (int p = 0; p < st.pellets; p++) {
        // Pass shotsFired for predictable spray mapping
        Vector3 dir = ApplySpread(look, inaccuracy, ws.shotsFired - 1, ws.id, speed);
        ShotResult sr = FireRay(eye, dir, st.range, shooter.id, shooter.team, world);

        // Register hit
        if (sr.hitPawn) {
            Pawn& target = world.pawns[sr.hitPawnID];
            target.hp = std::max(0, target.hp - st.damage);
            if (target.hp <= 0) target.alive = false;

            // Hit flash for player
            if (sr.hitPawnID == world.playerID)
                world.hitIndicatorAlpha = 1.0f;
        }
        else if (sr.hitGeom) {
            if ((int)world.impacts.size() < MAX_IMPACTS)
                world.impacts.push_back({ sr.endPoint, 3.0f });
        }

        // Bullet tracer visually starts from the gun tip, but mechanically fires from the eye
        if ((int)world.tracers.size() < MAX_TRACERS) {
            Color tc = (shooter.id == world.playerID) ? Color{ 255, 240, 160, 220 }
            : Color{ 255, 140, 100, 200 };
            world.tracers.push_back({ shooter.gunTip(), sr.endPoint, 0.06f, tc });
        }
    }

    // Auto-reload on empty
    if (ws.ammoMag == 0 && ws.ammoReserve > 0)
        ws.reloadTimer = st.reloadTimeSec;
}

// ─── Per-frame weapon tick ─────────────────────────────────────────────────────
inline void WeaponTick(WeaponState& ws, float dt) {
    if (ws.fireCooldown > 0)
        ws.fireCooldown = std::max(0.0f, ws.fireCooldown - dt);

    ws.timeSinceLastShot += dt;
    if (ws.timeSinceLastShot > 0.4f) {
        // Rapid reset or smooth decay of shotsFired
        ws.shotsFired = 0;
    }

    if (ws.reloadTimer > 0) {
        ws.reloadTimer -= dt;
        if (ws.reloadTimer <= 0) {
            const auto& st = ws.stats();
            int need = st.magSize - ws.ammoMag;
            int take = std::min(need, ws.ammoReserve);
            ws.ammoMag += take;
            ws.ammoReserve -= take;
            ws.reloadTimer = 0.0f;
        }
    }
}

// ─── Throw utility ─────────────────────────────────────────────────────────────
inline bool ThrowUtility(Pawn& thrower, UtilityID type, World& world) {
    int& count = (type == UtilityID::FRAG) ? thrower.fragCount
        : (type == UtilityID::SMOKE) ? thrower.smokeCount
        : thrower.stunCount;
    if (count <= 0 || (int)world.grenades.size() >= MAX_GRENADES) return false;
    count--;

    float   fuse = (type == UtilityID::FRAG) ? FRAG_FUSE_SEC : 0.8f;
    Vector3 vel = Vector3Scale(thrower.lookDir(), 12.0f);
    vel.y += 4.0f; // arc upward

    world.grenades.push_back({ type, thrower.eyePos(), vel, fuse, false, 0.0f, thrower.id });
    return true;
}