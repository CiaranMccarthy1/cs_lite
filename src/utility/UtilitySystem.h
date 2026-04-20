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
constexpr float GRENADE_RADIUS = 0.10f;
constexpr float GRENADE_STOP_SPEED = 0.35f;

// ─── Per-frame update ─────────────────────────────────────────────────────────
inline void UpdateUtility(World& world, float dt) {

    // ── Grenade flight ──────────────────────────────────────────────────────
    for(auto& g : world.grenades) {
        if(g.detonated) continue;

        g.fuseTimer -= dt;
        float moveDist = Vector3Length(g.vel) * dt;
        int subSteps = std::max(1, (int)ceilf(moveDist / 0.4f));
        float stepDt = dt / (float)subSteps;

        for(int step = 0; step < subSteps; step++) {
            Vector3 prevPos = g.pos;

            // Physics: sub-step integration avoids tunneling at high speed.
            g.vel.y += GRENADE_GRAVITY * stepDt;
            g.pos.x += g.vel.x * stepDt;
            g.pos.y += g.vel.y * stepDt;
            g.pos.z += g.vel.z * stepDt;

            bool collided = false;

            // Ground collision
            if(g.pos.y < GRENADE_RADIUS) {
                g.pos.y = GRENADE_RADIUS;
                g.vel.y = -g.vel.y * GRENADE_BOUNCE;
                g.vel.x *= 0.80f;
                g.vel.z *= 0.80f;
                collided = true;
            }

            BoundingBox gBox = {
                {g.pos.x - GRENADE_RADIUS, g.pos.y - GRENADE_RADIUS, g.pos.z - GRENADE_RADIUS},
                {g.pos.x + GRENADE_RADIUS, g.pos.y + GRENADE_RADIUS, g.pos.z + GRENADE_RADIUS}
            };

            for(const auto& s : world.solids) {
                if(!CheckCollisionBoxes(gBox, s.bounds)) continue;

                float penLeft   = fabsf(gBox.max.x - s.bounds.min.x);
                float penRight  = fabsf(s.bounds.max.x - gBox.min.x);
                float penBack   = fabsf(gBox.max.z - s.bounds.min.z);
                float penFront  = fabsf(s.bounds.max.z - gBox.min.z);
                float penBottom = fabsf(gBox.max.y - s.bounds.min.y);
                float penTop    = fabsf(s.bounds.max.y - gBox.min.y);

                float minPenX = std::min(penLeft, penRight);
                float minPenZ = std::min(penBack, penFront);
                float minPenY = std::min(penBottom, penTop);

                g.pos = prevPos;
                if(minPenY <= minPenX && minPenY <= minPenZ) {
                    g.vel.y = -g.vel.y * GRENADE_BOUNCE;
                    g.vel.x *= 0.90f;
                    g.vel.z *= 0.90f;
                } else if(minPenX < minPenZ) {
                    g.vel.x = -g.vel.x * GRENADE_BOUNCE;
                    g.vel.z *= 0.85f;
                } else {
                    g.vel.z = -g.vel.z * GRENADE_BOUNCE;
                    g.vel.x *= 0.85f;
                }

                collided = true;
                break;
            }

            // Settle almost-stopped grenades on the floor to avoid jitter.
            if(collided) {
                float planarSpeed = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
                if(planarSpeed < GRENADE_STOP_SPEED && fabsf(g.vel.y) < 1.0f &&
                   g.pos.y <= GRENADE_RADIUS + 0.02f) {
                    g.vel = {0, 0, 0};
                }
            }
        }

        // Detonate on fuse expiry
        if(g.fuseTimer <= 0) {
            g.detonated = true;

            switch(g.type) {
            // ── FRAG ─────────────────────────────────────────────────────
            case UtilityID::FRAG: {
                Team ownerTeam = Team::NONE;
                if(g.ownerID >= 0 && g.ownerID < MAX_PAWNS)
                    ownerTeam = world.pawns[g.ownerID].team;

                for(auto& pawn : world.pawns) {
                    if(!pawn.alive) continue;
                    if(!FRIENDLY_FIRE && ownerTeam != Team::NONE &&
                       pawn.team == ownerTeam && pawn.id != g.ownerID) {
                        continue;
                    }
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
                    if(!pawn.alive || pawn.isBot) continue;

                    Vector3 eye = pawn.eyePos();
                    Vector3 toFlash = Vector3Subtract(g.pos, eye);
                    float d = Vector3Length(toFlash);
                    float maxStunRange = FRAG_RADIUS * 1.5f;
                    if(d > maxStunRange || d < 0.01f) continue;

                    Vector3 dirToFlash = Vector3Scale(toFlash, 1.0f / d);
                    HitResult hr = RaycastSolids(eye, dirToFlash, d, world.solids);
                    bool blocked = hr.hit && hr.distance < d - 0.1f;
                    if(blocked) continue;

                    float facing = Vector3DotProduct(pawn.lookDir(), dirToFlash);
                    float facingScale = 0.2f + 0.8f * std::max(0.0f, facing);
                    float distScale = 1.0f - (d / maxStunRange);
                    float stunScale = std::clamp(facingScale * distScale * 1.2f, 0.0f, 1.0f);
                    float stunTime = STUN_DURATION_SEC * stunScale;

                    if(stunTime > world.stun.timeLeft) {
                        world.stun.timeLeft = stunTime;
                        world.stun.peak = std::max(0.2f, stunTime);
                    }
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
    if(world.stun.timeLeft > 0) {
        world.stun.timeLeft = std::max(0.0f, world.stun.timeLeft - dt);
        if(world.stun.timeLeft <= 0.0f)
            world.stun.peak = STUN_DURATION_SEC;
    }

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

    // ── Impact decay ──────────────────────────────────────────────────────────
    for(auto& imp : world.impacts) imp.lifeSec -= dt;
    world.impacts.erase(
        std::remove_if(world.impacts.begin(), world.impacts.end(),
            [](const ImpactDecal& imp){ return imp.lifeSec <= 0; }),
        world.impacts.end()
    );
}
