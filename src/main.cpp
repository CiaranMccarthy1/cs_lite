// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  –  TacticalLite entry point
//  Raspberry Pi 4 / OpenGL ES 2.0 / Raylib 4.5+
// ─────────────────────────────────────────────────────────────────────────────
#include <raylib.h>
#include <raymath.h>
#include <cstdlib>
#include <ctime>

#include "Constants.h"
#include "World.h"
#include "game/MapLoader.h"
#include "game/Physics.h"
#include "game/InputSystem.h"
#include "game/RoundManager.h"
#include "ai/BotAI.h"
#include "utility/UtilitySystem.h"
#include "render/Renderer.h"

// ─── Pi 4 specific: force GLES2 context before window creation ───────────────
static void ConfigurePi() {
#if defined(PLATFORM_RPI) || defined(PLATFORM_LINUX)
    // Mesa V3D driver hint — set MESA_GL_VERSION_OVERRIDE in your launch script:
    //   MESA_GL_VERSION_OVERRIDE=2.1 ./TacticalLite
    // This is just a reminder; actual env var is set in the launch script.
#endif
}

int main() {
    srand((unsigned)time(nullptr));
    ConfigurePi();

    // ── Window ────────────────────────────────────────────────────────────
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(RENDER_W, RENDER_H, "TacticalLite – 3v3 MVP");
    SetTargetFPS(TARGET_FPS);
    DisableCursor();  // lock + hide mouse for FPS look

    InitAudioDevice();

    // ── World & systems ───────────────────────────────────────────────────
    World    world;
    Renderer renderer;
    renderer.Init();

    // Load map
    MapData md;
    try {
        md = LoadMap("assets/maps/map01.map", world);
    } catch(std::exception& e) {
        TraceLog(LOG_WARNING, "Map load failed: %s — using procedural fallback", e.what());
        // ── Procedural fallback map ───────────────────────────────────────
        // Floor
        world.solids.push_back({{
            {-25, -0.2f, -25}, {25, 0.0f, 25}
        }, COL_FLOOR, true});
        // Outer walls
        world.solids.push_back({{{-25,0,-25},{-24.5f,4,25}}, COL_WALL});
        world.solids.push_back({{{24.5f,0,-25},{25,4,25}}, COL_WALL});
        world.solids.push_back({{{-25,0,-25},{25,4,-24.5f}}, COL_WALL});
        world.solids.push_back({{{-25,0,24.5f},{25,4,25}}, COL_WALL});
        // Some cover boxes
        world.solids.push_back({{{-3,0,-2},{-1,1.2f,2}}, {110,80,60,255}});
        world.solids.push_back({{{ 1,0,-2},{ 3,1.2f,2}}, {110,80,60,255}});
        world.solids.push_back({{{-8,0, 3},{-6,2.5f,5}}, {80,90,80,255}});
        world.solids.push_back({{{ 6,0, 3},{ 8,2.5f,5}}, {80,90,80,255}});
        // Waypoints
        world.waypoints = {
            {{-10,0, -8}, {1}},
            {{  0,0, -8}, {0,2}},
            {{ 10,0, -8}, {1,3}},
            {{ 10,0,  0}, {2,4}},
            {{  5,0,  5}, {3,5}},
            {{ -5,0,  5}, {4,0}},
        };
        // Objective
        world.objective = { {5,0,8}, 3.0f, 0, false };
        // Spawn points
        md.spawns = {
            { Team::ATTACK, {-12,0,-15}, 0.0f },
            { Team::ATTACK, {-14,0,-13}, 0.2f },
            { Team::ATTACK, {-10,0,-13}, -0.2f},
            { Team::DEFEND, { 12,0, 12}, (float)PI },
            { Team::DEFEND, { 14,0, 10}, (float)PI - 0.2f },
            { Team::DEFEND, { 10,0, 10}, (float)PI + 0.2f },
        };
    }

    // Initial round
    ResetRound(world, md);

    // ── Performance counters ──────────────────────────────────────────────
    double frameTimeAccum = 0;
    int    frameCount     = 0;
    float  displayFPS     = 0;

    // ── Main loop ─────────────────────────────────────────────────────────
    while(!WindowShouldClose()) {
        float dt = GetFrameTime();
        // Clamp dt to avoid spiral-of-death on slow frames
        if(dt > 0.05f) dt = 0.05f;

        // ── ESC to pause / quit ───────────────────────────────────────────
        if(IsKeyPressed(KEY_ESCAPE)) {
            EnableCursor();
            // Simple pause: wait for ESC again
            while(!WindowShouldClose()) {
                if(IsKeyPressed(KEY_ESCAPE)) break;
                BeginDrawing();
                ClearBackground(BLACK);
                DrawText("PAUSED  –  ESC to resume", 80, 320, 32, WHITE);
                DrawText("Press ESC to return to game", 80, 370, 22, GRAY);
                EndDrawing();
            }
            DisableCursor();
        }

        // ── Simulation updates ────────────────────────────────────────────
        ProcessInput(world, dt);
        UpdateRound(world, md, dt);
        if(world.roundState == RoundState::ACTIVE) {
            UpdateBots(world, dt);
            UpdateUtility(world, dt);
        }

        // ── Sync camera ───────────────────────────────────────────────────
        renderer.SyncCamera(world.player());

        // ── FPS counter ───────────────────────────────────────────────────
        frameTimeAccum += dt;
        frameCount++;
        if(frameTimeAccum >= 0.5) {
            displayFPS     = (float)frameCount / (float)frameTimeAccum;
            frameTimeAccum = 0;
            frameCount     = 0;
        }

        // ── Draw ──────────────────────────────────────────────────────────
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        BeginDrawing();
            renderer.DrawFrame(world, sw, sh);

            // FPS overlay (top-left, small)
            char fpsStr[16];
            snprintf(fpsStr, sizeof(fpsStr), "%.0f fps", displayFPS);
            DrawText(fpsStr, 8, 8, 16,
                     displayFPS >= 55 ? GREEN : (displayFPS >= 40 ? YELLOW : RED));

            // Dead overlay
            if(!world.player().alive && world.roundState == RoundState::ACTIVE) {
                DrawRectangle(0,0,sw,sh,{0,0,0,120});
                DrawText("YOU DIED", sw/2 - MeasureText("YOU DIED",52)/2,
                         sh/2 - 60, 52, RED);
            }

            // Match over screen
            if(world.roundState == RoundState::MATCH_OVER) {
                DrawRectangle(0,0,sw,sh,{0,0,0,200});
                const char* winner = world.scoreAttack > world.scoreDefend
                                     ? "ATTACKERS WIN THE MATCH!"
                                     : "DEFENDERS WIN THE MATCH!";
                DrawText(winner, sw/2 - MeasureText(winner,40)/2, sh/2-60, 40, YELLOW);
                char scores[32];
                snprintf(scores, sizeof(scores), "%d – %d",
                         world.scoreAttack, world.scoreDefend);
                DrawText(scores, sw/2 - MeasureText(scores,32)/2, sh/2, 32, WHITE);
                DrawText("Press ENTER to play again", sw/2 - 160, sh/2+60, 24, LIGHTGRAY);
            }
        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    renderer.Shutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
