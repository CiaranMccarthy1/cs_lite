// ─────────────────────────────────────────────────────────────────────────────
//  main.cpp  –  TacticalLite entry point
//  Raspberry Pi 4 / OpenGL ES 2.0 / Raylib 4.5+
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdlib>
#include <ctime>
#include <raylib.h>
#include <raymath.h>

#include "Constants.h"
#include "World.h"
#include "ai/BotAI.h"
#include "audio/AudioSystem.h"
#include "game/InputSystem.h"
#include "game/MapLoader.h"
#include "game/Physics.h"
#include "game/RoundManager.h"
#include "render/Renderer.h"
#include "ui/MenuSystem.h"
#include "utility/UtilitySystem.h"

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
  DisableCursor(); // lock + hide mouse for FPS look

  InitAudioDevice();
  AudioSystem audio;
  audio.Init();

  // ── World & systems ───────────────────────────────────────────────────
  World world;
  Renderer renderer;
  renderer.Init();

  // Load map
  MapData md;
  try {
    md = LoadMap("assets/maps/map01.map", world);
  } catch (std::exception &e) {
    TraceLog(LOG_WARNING, "Map load failed: %s — using procedural fallback",
             e.what());
    // ── Procedural fallback map ───────────────────────────────────────
    // Floor
    world.solids.push_back(
        {{{-25, -0.2f, -25}, {25, 0.0f, 25}}, COL_FLOOR, true});
    // Outer walls
    world.solids.push_back({{{-25, 0, -25}, {-24.5f, 4, 25}}, COL_WALL});
    world.solids.push_back({{{24.5f, 0, -25}, {25, 4, 25}}, COL_WALL});
    world.solids.push_back({{{-25, 0, -25}, {25, 4, -24.5f}}, COL_WALL});
    world.solids.push_back({{{-25, 0, 24.5f}, {25, 4, 25}}, COL_WALL});
    // Some cover boxes
    world.solids.push_back({{{-3, 0, -2}, {-1, 1.2f, 2}}, {110, 80, 60, 255}});
    world.solids.push_back({{{1, 0, -2}, {3, 1.2f, 2}}, {110, 80, 60, 255}});
    world.solids.push_back({{{-8, 0, 3}, {-6, 2.5f, 5}}, {80, 90, 80, 255}});
    world.solids.push_back({{{6, 0, 3}, {8, 2.5f, 5}}, {80, 90, 80, 255}});
    // Waypoints
    world.waypoints = {
        {{-10, 0, -8}, {1}},  {{0, 0, -8}, {0, 2}}, {{10, 0, -8}, {1, 3}},
        {{10, 0, 0}, {2, 4}}, {{5, 0, 5}, {3, 5}},  {{-5, 0, 5}, {4, 0}},
    };
    // Objective
    world.objective = {{5, 0, 8}, 3.0f, 0, false};
    // Spawn points
    md.spawns = {
        {Team::ATTACK, {-12, 0.1f, -15}, 0.0f},
        {Team::ATTACK, {-14, 0.1f, -13}, 0.2f},
        {Team::ATTACK, {-10, 0.1f, -13}, -0.2f},
        {Team::DEFEND, {12, 0.1f, 12}, (float)PI},
        {Team::DEFEND, {14, 0.1f, 10}, (float)PI - 0.2f},
        {Team::DEFEND, {10, 0.1f, 10}, (float)PI + 0.2f},
    };
  }

  // Initial round
  ResetRound(world, md);

  MenuSystem menu;
  bool quitIntent = false;

  // ── Performance counters ──────────────────────────────────────────────
  double frameTimeAccum = 0;
  int frameCount = 0;
  float displayFPS = 0;

  // Enable cursor at startup for main menu
  EnableCursor();

  // ── Main loop ─────────────────────────────────────────────────────────
  while (!WindowShouldClose() && !quitIntent) {
    float dt = GetFrameTime();
    // Clamp dt to avoid spiral-of-death on slow frames
    if (dt > 0.05f)
      dt = 0.05f;

    // ── ESC to pause / resume ─────────────────────────────────────────
    if (IsKeyPressed(KEY_ESCAPE) &&
        world.roundState != RoundState::MATCH_OVER) {
      if (menu.currentState == AppState::PLAYING) {
        menu.currentState = AppState::PAUSED;
        EnableCursor();
      } else if (menu.currentState == AppState::PAUSED) {
        menu.currentState = AppState::PLAYING;
        DisableCursor();
      }
    }

    // ── Update Logic ──────────────────────────────────────────────────
    if (menu.currentState == AppState::PLAYING) {
      ProcessInput(world, dt, audio);
      UpdateRound(world, md, dt);
      if (world.roundState == RoundState::ACTIVE) {
        UpdateBots(world, dt);
        UpdateUtility(world, dt);
      }

      // Check if game transitioned to match over internally
      if (world.roundState == RoundState::MATCH_OVER) {
        menu.currentState = AppState::MATCH_OVER;
        EnableCursor();
      }

      renderer.SyncCamera(world.player());
    } else if (menu.currentState == AppState::MATCH_OVER) {
      // Handle Match Over logic -> "Play Again" button overrides
      if (IsKeyPressed(KEY_ENTER)) {
        world.scoreAttack = 0;
        world.scoreDefend = 0;
        world.roundNumber = 1;
        ResetRound(world, md);
        menu.currentState = AppState::PLAYING;
        DisableCursor();
      }
    }

    // ── FPS counter ───────────────────────────────────────────────────
    frameTimeAccum += dt;
    frameCount++;
    if (frameTimeAccum >= 0.5) {
      displayFPS = (float)frameCount / (float)frameTimeAccum;
      frameTimeAccum = 0;
      frameCount = 0;
    }

    // ── Draw ──────────────────────────────────────────────────────────
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    BeginDrawing();

    if (menu.currentState == AppState::PLAYING ||
        menu.currentState == AppState::PAUSED ||
        menu.currentState == AppState::MATCH_OVER) {
      renderer.DrawFrame(world, sw, sh);

      // FPS overlay (top-left, small)
      char fpsStr[16];
      snprintf(fpsStr, sizeof(fpsStr), "%.0f fps", displayFPS);
      DrawText(fpsStr, 8, 8, 16,
               displayFPS >= 55 ? GREEN : (displayFPS >= 40 ? YELLOW : RED));

      // Dead overlay
      if (!world.player().alive && world.roundState == RoundState::ACTIVE) {
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 120});
        DrawText("YOU DIED", sw / 2 - MeasureText("YOU DIED", 52) / 2,
                 sh / 2 - 60, 52, RED);
      }
    }

    // Draw menus over the screen
    if (menu.currentState == AppState::MAIN_MENU) {
      menu.DrawMainMenu(sw, sh, quitIntent);
    } else if (menu.currentState == AppState::PAUSED) {
      DrawRectangle(0, 0, sw, sh,
                    {0, 0, 0, 180}); // Dim background deeper for pause
      menu.DrawPauseMenu(sw, sh, quitIntent);
    } else if (menu.currentState == AppState::MATCH_OVER) {
      DrawRectangle(0, 0, sw, sh, {0, 0, 0, 200});
      menu.DrawMatchOverScreen(sw, sh, world);

      // If the UI decided to replay
      if (world.roundState == RoundState::MATCH_OVER) {
        // wait... UI sets state to MATCH_OVER? Actually we should let play
        // again button reset
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(
                GetMousePosition(),
                {(float)(sw / 2 - 125), (float)(sh / 2 + 20), 250, 50})) {
          world.scoreAttack = 0;
          world.scoreDefend = 0;
          world.roundNumber = 1;
          ResetRound(world, md);
          menu.currentState = AppState::PLAYING;
          DisableCursor();
        }
      }
    }
    EndDrawing();
  }

  // ── Cleanup ───────────────────────────────────────────────────────────
  renderer.Shutdown();
  audio.Shutdown();
  CloseAudioDevice();
  CloseWindow();
  return 0;
}