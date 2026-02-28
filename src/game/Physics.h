#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Physics.h  –  AABB sweep + raycasts
//
//  Corner-clipping fix: each axis pass projects the movement delta onto that
//  axis only, so we never accidentally resolve a wall hit as a floor hit.
//  A SKIN constant keeps the player slightly away from surfaces so the
//  next frame never starts inside geometry.
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include <raymath.h>
#include <cmath>
#include <algorithm>

constexpr float PHYS_SKIN = 0.02f;   // gap kept between player and surfaces

inline Vector3 SweepAABB(
    Vector3                      pos,
    Vector3&                     vel,
    float                        dt,
    bool&                        hitFloor,
    const std::vector<MapSolid>& solids)
{
    hitFloor = false;
    const float R = PLAYER_RADIUS + PHYS_SKIN;
    const float H = PLAYER_HEIGHT;

    auto makeBox = [&](Vector3 p) -> BoundingBox {
        return { {p.x-R, p.y, p.z-R}, {p.x+R, p.y+H, p.z+R} };
    };

    // ── X pass ───────────────────────────────────────────────────────────────
    {
        Vector3 try1 = {pos.x + vel.x * dt, pos.y, pos.z};
        for(auto& s : solids) {
            BoundingBox box = makeBox(try1);
            if(!CheckCollisionBoxes(box, s.bounds)) continue;
            // Which side are we coming from?
            float overlapRight = s.bounds.max.x - (try1.x - R);  // entered from left
            float overlapLeft  = (try1.x + R) - s.bounds.min.x;  // entered from right
            if(overlapRight > 0 && overlapRight < overlapLeft)
                try1.x += overlapRight + PHYS_SKIN;
            else if(overlapLeft > 0)
                try1.x -= overlapLeft + PHYS_SKIN;
            vel.x = 0.0f;
        }
        pos.x = try1.x;
    }

    // ── Z pass ───────────────────────────────────────────────────────────────
    {
        Vector3 try1 = {pos.x, pos.y, pos.z + vel.z * dt};
        for(auto& s : solids) {
            BoundingBox box = makeBox(try1);
            if(!CheckCollisionBoxes(box, s.bounds)) continue;
            float overlapFwd  = s.bounds.max.z - (try1.z - R);
            float overlapBack = (try1.z + R) - s.bounds.min.z;
            if(overlapFwd > 0 && overlapFwd < overlapBack)
                try1.z += overlapFwd + PHYS_SKIN;
            else if(overlapBack > 0)
                try1.z -= overlapBack + PHYS_SKIN;
            vel.z = 0.0f;
        }
        pos.z = try1.z;
    }

    // ── Y pass ───────────────────────────────────────────────────────────────
    {
        Vector3 try1 = {pos.x, pos.y + vel.y * dt, pos.z};
        for(auto& s : solids) {
            BoundingBox box = makeBox(try1);
            if(!CheckCollisionBoxes(box, s.bounds)) continue;
            float overlapUp   = s.bounds.max.y - try1.y;         // floor below
            float overlapDown = (try1.y + H) - s.bounds.min.y;   // ceiling above
            if(overlapUp > 0 && overlapUp < overlapDown) {
                // Floor hit
                try1.y += overlapUp + PHYS_SKIN;
                hitFloor = true;
            } else if(overlapDown > 0) {
                // Ceiling hit
                try1.y -= overlapDown + PHYS_SKIN;
                vel.y = 0.0f;
            }
        }
        pos.y = try1.y;
    }

    // Hard floor safety net
    if(pos.y < 0.0f) {
        pos.y    = 0.0f;
        hitFloor = true;
    }

    return pos;
}

// ─── Raycast against world geometry ──────────────────────────────────────────
struct HitResult {
    bool    hit        = false;
    float   distance   = 0.0f;
    Vector3 point      = {};
    int     solidIndex = -1;
};

inline HitResult RaycastSolids(
    Vector3                      origin,
    Vector3                      direction,
    float                        maxDist,
    const std::vector<MapSolid>& solids)
{
    HitResult best;
    best.distance = maxDist + 1.0f;
    Ray ray = {origin, direction};
    for(int i = 0; i < (int)solids.size(); i++) {
        RayCollision rc = GetRayCollisionBox(ray, solids[i].bounds);
        if(rc.hit && rc.distance > 0.0f && rc.distance < best.distance) {
            best.hit        = true;
            best.distance   = rc.distance;
            best.point      = rc.point;
            best.solidIndex = i;
        }
    }
    if(best.distance > maxDist) best.hit = false;
    return best;
}

// ─── Smoke occlusion check ────────────────────────────────────────────────────
inline bool RayBlockedBySmoke(
    Vector3                       from,
    Vector3                       to,
    const std::vector<SmokeZone>& smokes)
{
    Vector3 dir = Vector3Subtract(to, from);
    float   len = Vector3Length(dir);
    if(len < 0.01f) return false;
    dir = Vector3Scale(dir, 1.0f / len);
    Ray ray = {from, dir};
    for(auto& smoke : smokes) {
        RayCollision rc = GetRayCollisionSphere(ray, smoke.pos, smoke.radius);
        if(rc.hit && rc.distance > 0.0f && rc.distance < len) return true;
    }
    return false;
}