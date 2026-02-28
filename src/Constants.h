#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  TacticalLite  –  Constants, enums and shared POD types
//  Targeting Raspberry Pi 4 (Cortex-A72 / OpenGL ES 2.0 / 1280×720)
// ─────────────────────────────────────────────────────────────────────────────
#include <raylib.h>
#include <raymath.h>
#include <cstdint>
#include <string>
#include <array>

// ─── Display ─────────────────────────────────────────────────────────────────
constexpr int   RENDER_W      = 1280;
constexpr int   RENDER_H      = 720;
constexpr int   TARGET_FPS    = 60;
constexpr float ASPECT        = (float)RENDER_W / (float)RENDER_H;

// ─── Teams ───────────────────────────────────────────────────────────────────
enum class Team : uint8_t { ATTACK = 0, DEFEND = 1, NONE = 2 };
constexpr int TEAM_SIZE = 3;

// ─── Round ───────────────────────────────────────────────────────────────────
constexpr float ROUND_TIME_SEC      = 90.0f;
constexpr float OBJECTIVE_CAPTURE_SEC = 10.0f;  // hold to win

// ─── Movement ────────────────────────────────────────────────────────────────
constexpr float PLAYER_SPEED       = 5.0f;   // m/s
constexpr float PLAYER_HEIGHT      = 1.75f;  // camera eye height
constexpr float PLAYER_RADIUS      = 0.4f;
constexpr float GRAVITY            = -18.0f;
constexpr float JUMP_VELOCITY      = 6.5f;
constexpr float MOUSE_SENSITIVITY  = 0.002f;

// ─── Camera ──────────────────────────────────────────────────────────────────
constexpr float CAM_FOV            = 75.0f;
constexpr float CAM_NEAR           = 0.05f;
constexpr float CAM_FAR            = 200.0f;

// ─── Weapons ─────────────────────────────────────────────────────────────────
enum class WeaponID : uint8_t {
    PISTOL  = 0,
    SMG     = 1,
    RIFLE   = 2,
    SNIPER  = 3,
    SHOTGUN = 4,
    COUNT
};

struct WeaponStats {
    const char* name;
    int         damage;          // HP per bullet hit
    int         magSize;
    float       fireRateRPM;     // rounds per minute
    float       reloadTimeSec;
    float       spreadRad;       // base crosshair spread (radians)
    float       adsSpreadMult;   // multiplier when ADS
    float       range;           // max raycast range (metres)
    int         pellets;         // shotgun: pellets per shot; else 1
    bool        semiAuto;        // true = one shot per click
};

// Indexed by WeaponID
constexpr std::array<WeaponStats, 5> WEAPON_TABLE = {{
    // name,      dmg, mag, RPM,   reload, spread,  adsMult, range, pel,  semi
    { "Pistol",    35,  12,  300, 1.5f,  0.030f, 0.40f,  80.0f,  1,  true  },
    { "SMG",       22,  25,  900, 2.0f,  0.080f, 0.60f,  50.0f,  1,  false },
    { "Rifle",     30,  30,  600, 2.2f,  0.020f, 0.30f, 150.0f,  1,  false },
    { "Sniper",   100,   5,   40, 3.5f,  0.005f, 0.10f, 300.0f,  1,  true  },
    { "Shotgun",   18,   6,  120, 2.8f,  0.200f, 0.50f,  20.0f, 8,  false },
}};

// ─── Utility ─────────────────────────────────────────────────────────────────
enum class UtilityID : uint8_t { FRAG = 0, SMOKE = 1, STUN = 2 };

constexpr float FRAG_RADIUS        = 4.5f;
constexpr float FRAG_DAMAGE        = 80.0f;
constexpr float FRAG_FUSE_SEC      = 2.5f;
constexpr float SMOKE_DURATION_SEC = 12.0f;
constexpr float SMOKE_RADIUS       = 3.5f;
constexpr float STUN_DURATION_SEC  = 2.0f;

// ─── AI ──────────────────────────────────────────────────────────────────────
constexpr float BOT_VISION_RANGE   = 40.0f;
constexpr float BOT_VISION_DOT     = 0.50f;  // cos(60°) – FOV half-angle
constexpr float BOT_REACTION_MS    = 250.0f; // ms before shooting
constexpr float BOT_RAYCAST_HZ     = 10.0f;  // vision checks per second
constexpr float BOT_AIM_NOISE_RAD  = 0.04f;  // accuracy noise
constexpr float BOT_SPEED          = 3.5f;
constexpr float BOT_WAYPOINT_REACH = 1.0f;   // metres – "close enough"

// ─── Health ──────────────────────────────────────────────────────────────────
constexpr int MAX_HP = 100;

// ─── Colours (flat palette) ───────────────────────────────────────────────────
constexpr Color COL_ATTACK  = { 220,  80,  80, 255 };  // red
constexpr Color COL_DEFEND  = {  80, 150, 220, 255 };  // blue
constexpr Color COL_NEUTRAL = { 180, 180, 180, 255 };
constexpr Color COL_SMOKE   = { 160, 160, 160, 180 };
constexpr Color COL_FLOOR   = {  60,  60,  60, 255 };
constexpr Color COL_WALL    = {  90,  90, 100, 255 };
constexpr Color COL_OBJ     = { 220, 180,  40, 255 };
constexpr Color COL_SKY     = {  30,  30,  40, 255 };

// ─── Shared tiny math helpers ────────────────────────────────────────────────
inline Vector3 V3(float x, float y, float z) { return {x,y,z}; }
inline float   Lerp1(float a, float b, float t) { return a + (b-a)*t; }
