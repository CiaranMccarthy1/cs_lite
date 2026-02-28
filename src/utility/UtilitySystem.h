#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  UtilitySystem.h  –  Frag / Smoke / Stun grenade simulation
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/Physics.h"
#include <raymath.h>
#include <cmath>
#include <algorithm>

constexpr float GRENADE_BOUNCE = 0.45f;  // restitution
constexpr float GRENADE_GRAVITY= -18.0f;

// ─── Per-frame update ─────────────────────────────────────────────────────────
inline void UpdateUtility(World& world, float dt) {

    // ── Grenade flight ──────────────────────────────────────────────────────
    for(auto& g : world.grenades) {
        if(g.detonated) continue;

        g.fuseTimer -= dt;

        // Physics: simple Euler + bounce
        g.vel.y += GRENADE_GRAVITY * dt;
        g.pos.x += g.vel.x * dt;
        g.pos.y += g.vel.y * dt;
        g.pos.z += g.vel.z * dt;

        // Floor bounce
        if(g.pos.y < 0.1f) {
            g.pos.y = 0.1f;
            g.vel.y = -g.vel.y * GRENADE_BOUNCE;
            g.vel.x *= 0.8f;
            g.vel.z *= 0.8f;
        }

        // Wall bounce using AABB check
        BoundingBox gBox = {
            {g.pos.x-0.1f, g.pos.y-0.1f, g.pos.z-0.1f},
            {g.pos.x+0.1f, g.pos.y+0.1f, g.pos.z+0.1f}
        };
        for(auto& s : world.solids) {
            if(CheckCollisionBoxes(gBox, s.bounds)) {
                g.vel.x = -g.vel.x * GRENADE_BOUNCE;
                g.vel.z = -g.vel.z * GRENADE_BOUNCE;
            }
        }

        // Detonate on fuse expiry
        if(g.fuseTimer <= 0) {
            g.detonated = true;

            switch(g.type) {
            // ── FRAG ─────────────────────────────────────────────────────
            case UtilityID::FRAG: {
                for(auto& pawn : world.pawns) {
                    if(!pawn.alive) continue;
                    float d = Vector3Length(Vector3Subtract(pawn.xform.pos, g.pos));
                    if(d > FRAG_RADIUS) continue;
                    // Line-of-sight for frag damage
                    HitResult hr = RaycastSolids(g.pos,
                        Vector3Normalize(Vector3Subtract(pawn.xform.pos, g.pos)),
                        d, world.solids);
                    bool blocked = hr.hit && hr.distance < d - 0.1f;
                    if(!blocked) {
                        float falloff = 1.0f - (d / FRAG_RADIUS);
                        int   dmg     = (int)(FRAG_DAMAGE * falloff);
                        pawn.hp = std::max(0, pawn.hp - dmg);
                        if(pawn.hp <= 0) pawn.alive = false;
                        // Hit flash if player was hit
                        if(&pawn == &world.player())
                            world.hitIndicatorAlpha = 1.0f;
                    }
                }
                break;
            }
            // ── SMOKE ────────────────────────────────────────────────────
            case UtilityID::SMOKE: {
                if((int)world.smokes.size() < MAX_SMOKES)
                    world.smokes.push_back({ g.pos, SMOKE_RADIUS, SMOKE_DURATION_SEC });
                break;
            }
            // ── STUN ─────────────────────────────────────────────────────
            case UtilityID::STUN: {
                for(auto& pawn : world.pawns) {
                    if(!pawn.alive) continue;
                    float d = Vector3Length(Vector3Subtract(pawn.xform.pos, g.pos));
                    if(d > FRAG_RADIUS * 1.5f) continue;
                    // Only affect player (bots ignore stun visually)
                    if(!pawn.isBot)
                        world.stun.timeLeft = STUN_DURATION_SEC;
                }
                break;
            }
            }
        }
    }

    // Remove detonated grenades
    world.grenades.erase(
        std::remove_if(world.grenades.begin(), world.grenades.end(),
            [](const GrenadeEntity& g){ return g.detonated; }),
        world.grenades.end()
    );

    // ── Smoke decay ──────────────────────────────────────────────────────────
    for(auto& s : world.smokes) s.lifeLeft -= dt;
    world.smokes.erase(
        std::remove_if(world.smokes.begin(), world.smokes.end(),
            [](const SmokeZone& s){ return s.lifeLeft <= 0; }),
        world.smokes.end()
    );

    // ── Stun overlay decay ────────────────────────────────────────────────────
    if(world.stun.timeLeft > 0) world.stun.timeLeft -= dt;

    // ── Hit indicator decay ───────────────────────────────────────────────────
    if(world.hitIndicatorAlpha > 0)
        world.hitIndicatorAlpha = std::max(0.0f, world.hitIndicatorAlpha - dt * 2.5f);

    // ── Tracer decay ──────────────────────────────────────────────────────────
    for(auto& t : world.tracers) t.lifeSec -= dt;
    world.tracers.erase(
        std::remove_if(world.tracers.begin(), world.tracers.end(),
            [](const BulletTracer& t){ return t.lifeSec <= 0; }),
        world.tracers.end()
    );
}
