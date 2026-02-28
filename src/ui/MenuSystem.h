#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  MenuSystem.h  –  Immediate mode UI for Main Menu, Pause menu, and Match Over
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include <raylib.h>

enum class AppState { MAIN_MENU, PLAYING, PAUSED, MATCH_OVER };

struct MenuSystem {
  // Shared state
  AppState currentState = AppState::MAIN_MENU;

  // UI drawing helper
  bool DrawButton(Rectangle bounds, const char *text) {
    bool clicked = false;
    Vector2 mousePoint = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePoint, bounds);

    if (isHovered) {
      DrawRectangleRec(bounds, {80, 80, 90, 255}); // Hover color
      DrawRectangleLinesEx(bounds, 2, {200, 200, 200, 255});

      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        clicked = true;
      }
    } else {
      DrawRectangleRec(bounds, {50, 50, 60, 255}); // Normal color
      DrawRectangleLinesEx(bounds, 2, {100, 100, 100, 255});
    }

    int textW = MeasureText(text, 20);
    DrawText(text, bounds.x + bounds.width / 2 - textW / 2,
             bounds.y + bounds.height / 2 - 10, 20, WHITE);

    return clicked;
  }

  // Draw the main menu
  void DrawMainMenu(int sw, int sh, bool &quitIntent) {
    DrawRectangle(0, 0, sw, sh, {30, 30, 40, 255}); // Dim background

    int titleW = MeasureText("TACTICAL LITE", 60);
    DrawText("TACTICAL LITE", sw / 2 - titleW / 2, sh / 2 - 150, 60, WHITE);

    int bw = 200, bh = 50;
    int bx = sw / 2 - bw / 2;
    int by = sh / 2 - 20;

    if (DrawButton({(float)bx, (float)by, (float)bw, (float)bh}, "PLAY GAME")) {
      currentState = AppState::PLAYING;
      DisableCursor();
    }

    if (DrawButton({(float)bx, (float)by + 70, (float)bw, (float)bh}, "QUIT")) {
      quitIntent = true;
    }
  }

  // Draw the pause menu
  void DrawPauseMenu(int sw, int sh, bool &quitIntent) {
    // We assume the game behind is darkened by main.cpp
    int titleW = MeasureText("PAUSED", 60);
    DrawText("PAUSED", sw / 2 - titleW / 2, sh / 2 - 150, 60, WHITE);

    int bw = 200, bh = 50;
    int bx = sw / 2 - bw / 2;
    int by = sh / 2 - 20;

    if (DrawButton({(float)bx, (float)by, (float)bw, (float)bh}, "RESUME")) {
      currentState = AppState::PLAYING;
      DisableCursor();
    }

    if (DrawButton({(float)bx, (float)by + 70, (float)bw, (float)bh},
                   "MAIN MENU")) {
      currentState = AppState::MAIN_MENU;
    }

    if (DrawButton({(float)bx, (float)by + 140, (float)bw, (float)bh},
                   "QUIT TO DESKTOP")) {
      quitIntent = true;
    }
  }

  // Draw the match over screen
  void DrawMatchOverScreen(int sw, int sh, World &world) {
    // We assume main.cpp dims the screen
    const char *winner = world.scoreAttack > world.scoreDefend
                             ? "ATTACKERS WIN THE MATCH!"
                             : "DEFENDERS WIN THE MATCH!";
    DrawText(winner, sw / 2 - MeasureText(winner, 40) / 2, sh / 2 - 120, 40,
             YELLOW);

    char scores[32];
    snprintf(scores, sizeof(scores), "ATK %d  –  %d DEF", world.scoreAttack,
             world.scoreDefend);
    DrawText(scores, sw / 2 - MeasureText(scores, 32) / 2, sh / 2 - 60, 32,
             WHITE);

    int bw = 250, bh = 50;
    int bx = sw / 2 - bw / 2;
    int by = sh / 2 + 20;

    if (DrawButton({(float)bx, (float)by, (float)bw, (float)bh},
                   "PLAY AGAIN")) {
      // Signal main to reset match properly
      world.roundState =
          RoundState::MATCH_OVER; // this actually gets handled by RoundManager
                                  // right now ideally but we can override it
    }

    if (DrawButton({(float)bx, (float)by + 70, (float)bw, (float)bh},
                   "MAIN MENU")) {
      currentState = AppState::MAIN_MENU;
    }
  }
};
