#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Physics.h  –  AABB sweep + gravity; no 3rd-party library
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>

// Sweep a moving AABB against all world solids.
// Returns the resolved position and sets onGround.
inline Vector3 SweepAABB(
    Vector3            pos,
    Vector3            vel,
    float              dt,
    bool&              onGround,
    const std::vector<MapSolid>& solids)
{
    onGround = false;
    Vector3 delta = Vector3Scale(vel, dt);

    // Axis-separated sweep (X, then Z, then Y) avoids corner-clipping
    constexpr float SKIN = 0.001f;

    auto testAxis = [&](Vector3 tryPos, int axis) -> float {
        float r  = PLAYER_RADIUS;
        float h  = PLAYER_HEIGHT;
        BoundingBox box = {
            { tryPos.x - r, tryPos.y,       tryPos.z - r },
            { tryPos.x + r, tryPos.y + h,   tryPos.z + r }
        };
        for(auto& s : solids) {
            if(CheckCollisionBoxes(box, s.bounds)) {
                // push out on this axis
                float penetration = 0;
                if(axis == 0) {
                    float overlap0 = s.bounds.max.x - (tryPos.x - r);
                    float overlap1 = (tryPos.x + r) - s.bounds.min.x;
                    penetration = (overlap0 < overlap1) ? overlap0 : -overlap1;
                } else if(axis == 1) {
                    float overlap0 = s.bounds.max.y - tryPos.y;
                    float overlap1 = (tryPos.y + h) - s.bounds.min.y;
                    if(overlap0 < overlap1) { penetration = overlap0; onGround = s.isFloor || overlap0 < 0.1f; }
                    else                   { penetration = -overlap1; }
                } else {
                    float overlap0 = s.bounds.max.z - (tryPos.z - r);
                    float overlap1 = (tryPos.z + r) - s.bounds.min.z;
                    penetration = (overlap0 < overlap1) ? overlap0 : -overlap1;
                }
                return penetration;
            }
        }
        return 0.0f;
    };

    // X axis
    Vector3 tryX = { pos.x + delta.x, pos.y, pos.z };
    float px = testAxis(tryX, 0);
    tryX.x += px;
    pos.x = tryX.x;

    // Z axis
    Vector3 tryZ = { pos.x, pos.y, pos.z + delta.z };
    float pz = testAxis(tryZ, 2);
    tryZ.z += pz;
    pos.z = tryZ.z;

    // Y axis
    Vector3 tryY = { pos.x, pos.y + delta.y, pos.z };
    float py = testAxis(tryY, 1);
    if(py != 0) delta.y = 0; // zero out velocity on collision
    tryY.y += py;
    if(py > -SKIN) onGround = true;
    pos.y = tryY.y;

    // Hard floor at y=0 as a safety net
    if(pos.y < 0) { pos.y = 0; onGround = true; }

    return pos;
}

// ─── Raycast against all world solids ────────────────────────────────────────
struct HitResult {
    bool    hit        = false;
    float   distance   = 0.0f;
    Vector3 point      = {};
    int     solidIndex = -1;
};

inline HitResult RaycastSolids(
    Vector3 origin,
    Vector3 direction,
    float   maxDist,
    const std::vector<MapSolid>& solids)
{
    HitResult best;
    best.distance = maxDist + 1.0f;
    Ray ray = { origin, direction };

    for(int i = 0; i < (int)solids.size(); i++) {
        RayCollision rc = GetRayCollisionBox(ray, solids[i].bounds);
        if(rc.hit && rc.distance > 0 && rc.distance < best.distance) {
            best.hit        = true;
            best.distance   = rc.distance;
            best.point      = rc.point;
            best.solidIndex = i;
        }
    }
    if(best.distance > maxDist) best.hit = false;
    return best;
}

// ─── Check if a smoke zone occludes a ray from A to B ────────────────────────
inline bool RayBlockedBySmoke(
    Vector3 from, Vector3 to,
    const std::vector<SmokeZone>& smokes)
{
    Vector3 dir = Vector3Subtract(to, from);
    float   len = Vector3Length(dir);
    if(len < 0.01f) return false;
    dir = Vector3Scale(dir, 1.0f / len);
    Ray ray = { from, dir };

    for(auto& smoke : smokes) {
        RayCollision rc = GetRayCollisionSphere(ray, smoke.pos, smoke.radius);
        if(rc.hit && rc.distance > 0 && rc.distance < len) return true;
    }
    return false;
}
