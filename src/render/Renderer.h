#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Renderer.h  –  Forward-rendered flat-shaded scene
//  All geometry is drawn as Raylib primitives or loaded models.
//  No shadow maps, no PBR; straight flat/unshaded colours → fast on Pi 4.
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../weapons/WeaponSystem.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

struct Renderer {
    RenderTexture2D renderTarget;   // 1280×720 offscreen
    Camera3D        cam3D;
    Font            uiFont;
    Model           viewmodelGun;

    void Init() {
        renderTarget = LoadRenderTexture(RENDER_W, RENDER_H);
        SetTextureFilter(renderTarget.texture, TEXTURE_FILTER_BILINEAR);

        cam3D = {};
        cam3D.up         = {0,1,0};
        cam3D.fovy       = CAM_FOV;
        cam3D.projection = CAMERA_PERSPECTIVE;

        uiFont = GetFontDefault();
        viewmodelGun = LoadModelFromMesh(GenMeshCube(0.1f, 0.15f, 0.4f));
    }

    void Shutdown() {
        UnloadModel(viewmodelGun);
        UnloadRenderTexture(renderTarget);
    }

    // ── Sync camera to player ──────────────────────────────────────────────
    void SyncCamera(const Pawn& player, float dt) {
        float targetFov = CAM_FOV;
        if(player.alive && player.weapon.isADS) {
            targetFov = (player.weapon.id == WeaponID::SNIPER) ? 28.0f : 58.0f;
        }

        float fovLerp = std::clamp(dt * 14.0f, 0.0f, 1.0f);
        cam3D.fovy = Lerp1(cam3D.fovy, targetFov, fovLerp);

        cam3D.position = player.eyePos();
        cam3D.target   = Vector3Add(cam3D.position, player.lookDir());
    }

    // ── Draw everything ─────────────────────────────────────────────────────
    void DrawFrame(const World& world, int screenW, int screenH) {
        // ── 1. Render 3D scene to offscreen texture ─────────────────────────
        BeginTextureMode(renderTarget);
        ClearBackground(COL_SKY);

        BeginMode3D(cam3D);

            DrawMap(world);
            DrawPawns(world);
            DrawGrenades(world);
            DrawSmokes(world);
            DrawTracers(world);
            DrawImpacts(world);
            DrawObjective(world);

        EndMode3D();

        // Render viewmodel in a separate pass so it never clips into the map.
        BeginMode3D(cam3D);
        rlDisableDepthTest();
        DrawViewmodel(world);
        rlEnableDepthTest();
        EndMode3D();
        EndTextureMode();

        // ── 2. Blit scaled to screen ─────────────────────────────────────────
        // Source flipped on Y because OpenGL textures are bottom-up
        Rectangle src = { 0, 0, (float)RENDER_W, -(float)RENDER_H };
        Rectangle dst = { 0, 0, (float)screenW,  (float)screenH   };
        DrawTexturePro(renderTarget.texture, src, dst, {0,0}, 0, WHITE);

        // ── 3. HUD (drawn at native resolution) ──────────────────────────────
        DrawHUD(world, screenW, screenH);
    }

    // ─── Draw local player Viewmodel ─────────────────────────────────────────
    void DrawViewmodel(const World& world) {
        const Pawn& p = world.player();
        if(!p.alive) return;

        Vector3 eye = p.eyePos();

        rlPushMatrix();
        rlTranslatef(eye.x, eye.y, eye.z);

        // Raylib yaw=0 is +Z. We yaw around Y axis.
        rlRotatef(p.xform.yaw * RAD2DEG, 0, 1, 0);
        // Pitch: rotate around X. 
        // Our pitch logic: larger pitch views UP. LookDir has y=sin(pitch).
        // Standard rlRotatef for X is counter-clockwise. Viewing from +X, +Z to +Y is CCW, so negative.
        rlRotatef(-p.xform.pitch * RAD2DEG, 1, 0, 0);

        // Translate the bounds of where we hold it exactly relative to our math
        // Center of the model is 0.5 relative to the camera origin.
        rlTranslatef(0.3f, -0.25f, 0.5f);

        DrawModel(viewmodelGun, {0,0,0}, 1.0f, {80,80,90, 255});
        DrawModelWires(viewmodelGun, {0,0,0}, 1.01f, {30,30,40, 255});

        rlPopMatrix();
    }

private:
    // ─── Map geometry ────────────────────────────────────────────────────────
    void DrawMap(const World& world) {
        for(auto& s : world.solids) {
            Vector3 center = {
                (s.bounds.min.x + s.bounds.max.x) * 0.5f,
                (s.bounds.min.y + s.bounds.max.y) * 0.5f,
                (s.bounds.min.z + s.bounds.max.z) * 0.5f
            };
            Vector3 size = {
                s.bounds.max.x - s.bounds.min.x,
                s.bounds.max.y - s.bounds.min.y,
                s.bounds.max.z - s.bounds.min.z
            };
            DrawCube(center, size.x, size.y, size.z, s.col);
            // Draw wire slightly larger to give edge definition (increased offset to stop Z-fighting jitter)
            DrawCubeWires(center, size.x + 0.04f, size.y + 0.04f, size.z + 0.04f,
                          { (unsigned char)(s.col.r/2),
                            (unsigned char)(s.col.g/2),
                            (unsigned char)(s.col.b/2), 120 });
        }

        // Waypoint debug dots (disable in release)
#if defined(SHOW_WAYPOINTS)
        for(auto& wp : world.waypoints) {
            DrawSphere(wp.pos, 0.15f, YELLOW);
            for(int nb : wp.neighbours)
                DrawLine3D(wp.pos, world.waypoints[nb].pos, { 255,255,0,100 });
        }
#endif
    }

    // ─── Pawns (capsule-like: cylinder body + sphere head) ──────────────────
    void DrawPawns(const World& world) {
        for(int i = 0; i < MAX_PAWNS; i++) {
            const Pawn& p = world.pawns[i];
            if(!p.alive || i == world.playerID) continue;  // skip dead & self

            Color bodyCol = (p.team == Team::ATTACK) ? COL_ATTACK : COL_DEFEND;
            Color darkCol = { (unsigned char)(bodyCol.r/2),
                              (unsigned char)(bodyCol.g/2),
                              (unsigned char)(bodyCol.b/2), 255 };

            // Body
            DrawCylinder(p.xform.pos, PLAYER_RADIUS, PLAYER_RADIUS,
                         p.height() * 0.8f, 6, bodyCol);
            // Head
            Vector3 headPos = { p.xform.pos.x,
                                p.xform.pos.y + p.height() * 0.9f,
                                p.xform.pos.z };
            DrawSphere(headPos, 0.22f, darkCol);
            // "Gun" stub
            Vector3 gunFwd = { sinf(p.xform.yaw) * 0.6f, 0.0f, cosf(p.xform.yaw) * 0.6f };
            Vector3 gunEnd = Vector3Add(Vector3Add(p.xform.pos,
                                Vector3{0, p.height()*0.55f, 0}), gunFwd);
            DrawLine3D(Vector3Add(p.xform.pos, {0, p.height()*0.55f, 0}),
                       gunEnd, RAYWHITE);

            // HP bar above head
            Vector3 barBase = { headPos.x, headPos.y + 0.35f, headPos.z };
            // (3D bar is tricky; defer to 2D HUD for simplicity)
        }
    }

    // ─── In-flight grenades ──────────────────────────────────────────────────
    void DrawGrenades(const World& world) {
        for(auto& g : world.grenades) {
            if(g.detonated) continue;
            Color c = (g.type == UtilityID::FRAG) ? Color{ 60,200,60,255 }
                : (g.type == UtilityID::SMOKE) ? Color{ 160,160,160,255 }
                : Color{ 240,240,60,255 };

            DrawSphere(g.pos, 0.12f, c);
        }
    }

    // ─── Smoke spheres ───────────────────────────────────────────────────────
    void DrawSmokes(const World& world) {
        for(auto& s : world.smokes) {
            float alpha = std::min(1.0f, s.lifeLeft / 2.0f); // fade out at the end
            // Full opacity during main lifetime (255) instead of transparent
            Color c = { 155, 155, 155, (unsigned char)(255 * alpha) };
            DrawSphere(s.pos, s.radius, c);
            // Inner denser core
            DrawSphere(s.pos, s.radius * 0.6f, { 130,130,130,(unsigned char)(255 * alpha) });
        }
    }

    // ─── Bullet tracers ──────────────────────────────────────────────────────
    void DrawTracers(const World& world) {
        for(auto& t : world.tracers) {
            float a = t.lifeSec / 0.06f;
            Color c = { t.col.r, t.col.g, t.col.b, (unsigned char)(t.col.a * a) };
            DrawLine3D(t.origin, t.end, c);
        }
    }

    // ─── Bullet holes / Impacts ──────────────────────────────────────────────
    void DrawImpacts(const World& world) {
        for(auto& imp : world.impacts) {
            float alpha = std::min(1.0f, imp.lifeSec); // Fade out last second
            Color c = { 10, 10, 10, (unsigned char)(255 * alpha) };
            // Draw a small distinct sphere for the impact point
            DrawSphere(imp.pos, 0.035f, c);
        }
    }

    // ─── Objective zone ──────────────────────────────────────────────────────
    void DrawObjective(const World& world) {
        Color c = world.objective.captured ? Color{80,255,80,180} : Color{220,180,40,140};
        // Pulsing ring on floor
        float pulse = 0.9f + 0.1f * sinf(GetTime() * 3.0f);
        DrawCircle3D(
            Vector3Add(world.objective.pos, {0, 0.05f, 0}),
            world.objective.radius * pulse,
            {1,0,0}, 90.0f,
            c
        );
        // Vertical pillar of light (thin cylinder)
        DrawCylinder(world.objective.pos, 0.05f, 0.05f, 3.0f, 6, c);
    }

    // ─── HUD ─────────────────────────────────────────────────────────────────
    void DrawHUD(const World& world, int sw, int sh) {
        const Pawn& p = world.player();

        // ── Hit indicator (red vignette) ──────────────────────────────────
        if(world.hitIndicatorAlpha > 0) {
            unsigned char a = (unsigned char)(world.hitIndicatorAlpha * 120);
            DrawRectangle(0, 0, sw, sh, {200, 30, 30, a});
        }

        // ── Stun overlay (white flash) ────────────────────────────────────
        if(world.stun.timeLeft > 0) {
            unsigned char a = (unsigned char)(world.stun.alpha() * 255);
            DrawRectangle(0, 0, sw, sh, {255, 255, 255, a});
        }

        // ── Crosshair ────────────────────────────────────────────────────
        int cx = sw/2, cy = sh/2;
        float crossInaccuracy = ComputeShotInaccuracy(p, p.weapon.isADS);
        bool scopedSniper = (p.weapon.id == WeaponID::SNIPER && p.weapon.isADS);
        if(!scopedSniper) {
            int cs = 6 + (int)(crossInaccuracy * 220.0f);
            cs = std::clamp(cs, 6, 42);
            DrawRectangle(cx - 1, cy - cs, 2, cs - 3, WHITE);
            DrawRectangle(cx - 1, cy + 3,  2, cs - 3, WHITE);
            DrawRectangle(cx - cs, cy - 1, cs - 3, 2,  WHITE);
            DrawRectangle(cx + 3,  cy - 1, cs - 3, 2,  WHITE);
            DrawRectangle(cx - 1, cy - 1, 2, 2, WHITE);
        } else {
            DrawLine(cx - 18, cy, cx + 18, cy, WHITE);
            DrawLine(cx, cy - 18, cx, cy + 18, WHITE);
        }

        float normalizedInaccuracy = std::clamp(crossInaccuracy / 0.60f, 0.0f, 1.0f);
        float accuracyPct = (1.0f - normalizedInaccuracy) * 100.0f;
        char accText[24];
        snprintf(accText, sizeof(accText), "ACC %.0f%%", accuracyPct);
        DrawText(accText, cx - MeasureText(accText, 16)/2, cy + 26, 16, LIGHTGRAY);

        // ── Ammo ─────────────────────────────────────────────────────────
        auto& ws = p.weapon;
        bool reloading = ws.reloadTimer > 0;
        char ammoText[32];
        if(reloading) snprintf(ammoText, sizeof(ammoText), "RELOADING…");
        else snprintf(ammoText, sizeof(ammoText), "%d / %d", ws.ammoMag, ws.ammoReserve);
        DrawText(ammoText, sw - 200, sh - 60, 26, WHITE);
        DrawText(ws.stats().name, sw - 200, sh - 90, 20, LIGHTGRAY);

        // ── HP bar ────────────────────────────────────────────────────────
        int barW = 200, barH = 18;
        int barX = 20, barY = sh - 40;
        DrawRectangle(barX, barY, barW, barH, DARKGRAY);
        DrawRectangle(barX, barY, (int)(barW * p.hp / (float)MAX_HP), barH,
                      p.hp > 40 ? GREEN : (p.hp > 20 ? ORANGE : RED));
        char hpText[16]; snprintf(hpText, sizeof(hpText), "HP %d", p.hp);
        DrawText(hpText, barX + 4, barY + 1, 16, WHITE);

        // ── Utility counts ────────────────────────────────────────────────
        char util[48];
        snprintf(util, sizeof(util), "F:%d  S:%d  ST:%d",
                 p.fragCount, p.smokeCount, p.stunCount);
        DrawText(util, 20, sh - 70, 18, LIGHTGRAY);

        // ── Round timer ───────────────────────────────────────────────────
        int secs = (int)world.roundTimer;
        char timerText[16]; snprintf(timerText, sizeof(timerText),
                                     "%d:%02d", secs/60, secs%60);
        int tw = MeasureText(timerText, 28);
        Color timerCol = (world.roundTimer < 15) ? RED : WHITE;
        DrawText(timerText, sw/2 - tw/2, 14, 28, timerCol);

        // ── Score ─────────────────────────────────────────────────────────
        char scoreText[32];
        snprintf(scoreText, sizeof(scoreText),
                 "ATK %d  –  DEF %d", world.scoreAttack, world.scoreDefend);
        int stw = MeasureText(scoreText, 20);
        DrawText(scoreText, sw/2 - stw/2, 48, 20, LIGHTGRAY);

        // ── Round state banner ────────────────────────────────────────────
        if(world.roundState == RoundState::WAITING) {
            const char* msg = "GET READY";
            int mw = MeasureText(msg, 48);
            DrawText(msg, sw/2 - mw/2, sh/2 - 60, 48, YELLOW);
        }
        else if(world.roundState == RoundState::ROUND_OVER) {
            const char* msg = (world.roundWinner == Team::ATTACK) ? "ATTACKERS WIN!"
                            : (world.roundWinner == Team::DEFEND) ? "DEFENDERS WIN!"
                            : "DRAW";
            int mw = MeasureText(msg, 48);
            DrawRectangle(0, sh/2 - 70, sw, 80, {0,0,0,160});
            DrawText(msg, sw / 2 - mw / 2, sh / 2 - 55, 48,
                world.roundWinner == Team::ATTACK ? Color{ 255,100,100,255 }
                : Color{ 100,150,255,255 });

        }

        // ── Mini-map (top-right, 120×120) ─────────────────────────────────
        DrawMinimap(world, sw - 130, 10, 120);

        // ── Objective capture bar ─────────────────────────────────────────
        if(!world.objective.captured) {
            float prog = world.objective.captureProgress / OBJECTIVE_CAPTURE_SEC;
            if(prog > 0) {
                int obW = 300, obH = 14;
                int obX = sw/2 - obW/2, obY = sh - 110;
                DrawRectangle(obX, obY, obW, obH, DARKGRAY);
                DrawRectangle(obX, obY, (int)(obW*prog), obH, COL_OBJ);
                DrawText("CAPTURING OBJECTIVE", obX, obY - 20, 16, COL_OBJ);
            }
        }
    }

    // ─── Mini-map ─────────────────────────────────────────────────────────────
    void DrawMinimap(const World& world, int ox, int oy, int size) {
        float minX = -25.0f, maxX = 25.0f;
        float minZ = -25.0f, maxZ = 25.0f;
        if(!world.solids.empty()) {
            minX = world.solids[0].bounds.min.x;
            maxX = world.solids[0].bounds.max.x;
            minZ = world.solids[0].bounds.min.z;
            maxZ = world.solids[0].bounds.max.z;
            for(const auto& s : world.solids) {
                minX = std::min(minX, s.bounds.min.x);
                maxX = std::max(maxX, s.bounds.max.x);
                minZ = std::min(minZ, s.bounds.min.z);
                maxZ = std::max(maxZ, s.bounds.max.z);
            }
        }

        float spanX = std::max(1.0f, maxX - minX);
        float spanZ = std::max(1.0f, maxZ - minZ);
        float worldSpan = std::max(spanX, spanZ);
        float mapScale = (size - 10.0f) / worldSpan;
        float centerX = (minX + maxX) * 0.5f;
        float centerZ = (minZ + maxZ) * 0.5f;

        DrawRectangle(ox, oy, size, size, {0,0,0,160});
        DrawRectangleLines(ox, oy, size, size, GRAY);

        auto wToMap = [&](float wx, float wz) -> Vector2 {
            return { ox + size/2.0f + (wx - centerX) * mapScale,
                     oy + size/2.0f + (wz - centerZ) * mapScale };
        };

        // Objective
        Vector2 objPt = wToMap(world.objective.pos.x, world.objective.pos.z);
        DrawCircleV(objPt, 4, COL_OBJ);

        // Smokes
        for(auto& s : world.smokes) {
            Vector2 sp = wToMap(s.pos.x, s.pos.z);
            DrawCircleV(sp, 5, {160,160,160,180});
        }

        // Pawns
        for(int i = 0; i < MAX_PAWNS; i++) {
            const Pawn& p = world.pawns[i];
            if(!p.alive) continue;
            Vector2 pp = wToMap(p.xform.pos.x, p.xform.pos.z);
            Color dc = (p.team == Team::ATTACK) ? COL_ATTACK : COL_DEFEND;
            if(i == world.playerID) { DrawRectangle((int)pp.x-3,(int)pp.y-3,6,6,WHITE); }
            else                    { DrawCircleV(pp, 3, dc); }
        }
    }
};
