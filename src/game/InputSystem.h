#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  InputSystem.h  –  Player movement, look, fire, utility keys
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/Physics.h"
#include "../weapons/WeaponSystem.h"
#include <raylib.h>
#include <raymath.h>
#include <cmath>

// Key bindings (prefixed BIND_ to avoid clashing with Raylib's KEY_LEFT/KEY_RIGHT/KEY_BACK enums)
constexpr KeyboardKey BIND_FWD    = KEY_W;
constexpr KeyboardKey BIND_BACK   = KEY_S;
constexpr KeyboardKey BIND_LEFT   = KEY_A;
constexpr KeyboardKey BIND_RIGHT  = KEY_D;
constexpr KeyboardKey BIND_JUMP   = KEY_SPACE;
constexpr KeyboardKey BIND_RELOAD = KEY_R;
constexpr KeyboardKey BIND_FRAG   = KEY_G;
constexpr KeyboardKey BIND_SMOKE  = KEY_T;
constexpr KeyboardKey BIND_STUN   = KEY_F;
// Weapon select: 1-5
constexpr MouseButton BTN_FIRE   = MOUSE_BUTTON_LEFT;
constexpr MouseButton BTN_ADS    = MOUSE_BUTTON_RIGHT;

inline void ProcessInput(World& world, float dt) {
    if(world.roundState == RoundState::WAITING ||
       world.roundState == RoundState::ROUND_OVER ||
       world.roundState == RoundState::MATCH_OVER) return;

    Pawn& player = world.player();
    if(!player.alive) return;

    // ── Mouse look ────────────────────────────────────────────────────────
    Vector2 md = GetMouseDelta();
    player.xform.yaw   += md.x * MOUSE_SENSITIVITY;
    player.xform.pitch -= md.y * MOUSE_SENSITIVITY;
    player.xform.pitch  = std::clamp(player.xform.pitch, -1.45f, 1.45f);

    // ── Movement ──────────────────────────────────────────────────────────
    Vector3 forward = { sinf(player.xform.yaw), 0, cosf(player.xform.yaw) };
    Vector3 right   = { forward.z, 0, -forward.x };

    Vector3 moveDir = {0,0,0};
    if(IsKeyDown(BIND_FWD))   moveDir = Vector3Add(moveDir, forward);
    if(IsKeyDown(BIND_BACK))  moveDir = Vector3Subtract(moveDir, forward);
    if(IsKeyDown(BIND_LEFT))  moveDir = Vector3Subtract(moveDir, right);
    if(IsKeyDown(BIND_RIGHT)) moveDir = Vector3Add(moveDir, right);

    bool sprinting = IsKeyDown(KEY_LEFT_SHIFT);
    float spd = PLAYER_SPEED * (sprinting ? 1.5f : 1.0f);

    if(Vector3Length(moveDir) > 0) {
        moveDir = Vector3Scale(Vector3Normalize(moveDir), spd);
    }
    player.velocity.x = moveDir.x;
    player.velocity.z = moveDir.z;

    // ── Gravity / jump ────────────────────────────────────────────────────
    if(!player.onGround) player.velocity.y += GRAVITY * dt;
    if(IsKeyPressed(BIND_JUMP) && player.onGround) player.velocity.y = JUMP_VELOCITY;

    // ── Sweep ─────────────────────────────────────────────────────────────
    bool og = player.onGround;
    player.xform.pos = SweepAABB(player.xform.pos, player.velocity, dt, og, world.solids);
    player.onGround  = og;
    if(og) player.velocity.y = 0;

    // ── Weapon select ─────────────────────────────────────────────────────
    for(int k = 0; k < 5; k++) {
        if(IsKeyPressed(KEY_ONE + k)) {
            player.weapon.id       = (WeaponID)k;
            player.weapon.ammoMag  = WEAPON_TABLE[k].magSize;
            player.weapon.ammoReserve = WEAPON_TABLE[k].magSize * 2;
            player.weapon.reloadTimer  = 0;
            player.weapon.fireCooldown = 0;
        }
    }

    // ── ADS ───────────────────────────────────────────────────────────────
    player.weapon.isADS = IsMouseButtonDown(BTN_ADS);

    // ── Fire ──────────────────────────────────────────────────────────────
    bool triggerDown = IsMouseButtonDown(BTN_FIRE);
    bool triggerPressed = IsMouseButtonPressed(BTN_FIRE);
    bool shouldFire = player.weapon.stats().semiAuto ? triggerPressed : triggerDown;

    if(shouldFire) WeaponFire(player, world, player.weapon.isADS);

    // ── Reload ────────────────────────────────────────────────────────────
    if(IsKeyPressed(BIND_RELOAD) && player.weapon.reloadTimer <= 0) {
        player.weapon.reloadTimer = player.weapon.stats().reloadTimeSec;
    }

    // ── Weapon tick ───────────────────────────────────────────────────────
    WeaponTick(player.weapon, dt);

    // ── Utility throws ────────────────────────────────────────────────────
    if(IsKeyPressed(BIND_FRAG))  ThrowUtility(player, UtilityID::FRAG,  world);
    if(IsKeyPressed(BIND_SMOKE)) ThrowUtility(player, UtilityID::SMOKE, world);
    if(IsKeyPressed(BIND_STUN))  ThrowUtility(player, UtilityID::STUN,  world);
}