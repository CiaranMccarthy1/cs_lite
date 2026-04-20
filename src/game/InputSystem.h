#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  InputSystem.h  –  Player movement, look, fire, utility keys
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../game/Physics.h"
#include "../weapons/WeaponSystem.h"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

// Key bindings (prefixed BIND_ to avoid clashing with Raylib's KEY_LEFT/KEY_RIGHT/KEY_BACK enums)
constexpr KeyboardKey BIND_FWD    = KEY_W;
constexpr KeyboardKey BIND_BACK   = KEY_S;
constexpr KeyboardKey BIND_LEFT   = KEY_A;
constexpr KeyboardKey BIND_RIGHT  = KEY_D;
constexpr KeyboardKey BIND_JUMP   = KEY_SPACE;
constexpr KeyboardKey BIND_CROUCH = KEY_LEFT_CONTROL;
constexpr KeyboardKey BIND_RELOAD = KEY_R;
constexpr KeyboardKey BIND_FRAG   = KEY_G;
constexpr KeyboardKey BIND_SMOKE  = KEY_T;
constexpr KeyboardKey BIND_STUN   = KEY_F;
constexpr MouseButton BTN_FIRE    = MOUSE_BUTTON_LEFT;
constexpr MouseButton BTN_ADS     = MOUSE_BUTTON_RIGHT;

inline void TrySwitchWeapon(Pawn &player, WeaponID nextId) {
    if ((int)nextId < 0 || (int)nextId >= (int)WeaponID::COUNT)
        return;
    player.equipWeapon(nextId);
}

inline void ProcessInput(World& world, float dt, AudioSystem& audio) {
    if(world.roundState == RoundState::ROUND_OVER ||
       world.roundState == RoundState::MATCH_OVER) return;

    Pawn& player = world.player();
    if(!player.alive) return;

    // ── Mouse look ────────────────────────────────────────────────────────
    Vector2 md = GetMouseDelta();
    // Allow looking around during waiting state, but disable movement
    if (world.roundState == RoundState::WAITING) {
        player.xform.yaw -= md.x * MOUSE_SENSITIVITY;   // Invert X axis
        player.xform.pitch -= md.y * MOUSE_SENSITIVITY;   // Invert Y axis
        player.xform.pitch = std::clamp(player.xform.pitch, -1.45f, 1.45f);
        return;
    }

    player.xform.yaw -= md.x * MOUSE_SENSITIVITY;   // Invert X axis
    player.xform.pitch -= md.y * MOUSE_SENSITIVITY;   // Invert Y axis
    player.xform.pitch = std::clamp(player.xform.pitch, -1.45f, 1.45f);


    // ── Horizontal movement (Bhop / Source Engine style) ──────────────────
    Vector3 forward = { sinf(player.xform.yaw), 0, cosf(player.xform.yaw) };
    Vector3 right   = { forward.z, 0, -forward.x };

    Vector3 wishDir = {0,0,0};
    if(IsKeyDown(BIND_FWD))   wishDir = Vector3Add(wishDir, forward);
    if(IsKeyDown(BIND_BACK))  wishDir = Vector3Subtract(wishDir, forward);
    if(IsKeyDown(BIND_LEFT))  wishDir = Vector3Subtract(wishDir, right);
    if(IsKeyDown(BIND_RIGHT)) wishDir = Vector3Add(wishDir, right);

    bool walking = IsKeyDown(KEY_LEFT_SHIFT);
    player.isCrouching = IsKeyDown(BIND_CROUCH);

    float maxSpeed = PLAYER_SPEED;
    if (player.isCrouching) {
        maxSpeed *= 0.34f; // Crouch speed modifier
    } else if (walking) {
        maxSpeed *= 0.52f; // Walk modifier
    }

    float wishLength = Vector3Length(wishDir);
    if(wishLength > 0.0f) {
        wishDir = Vector3Scale(wishDir, 1.0f / wishLength);
    }

    // ── Jump (Execute before friction so speed is preserved) ──────────────
    // Using IsKeyDown enables auto-bhopping on hold, removing the need for 
    // scroll-wheel macros which is standard for implementing bhop physics 
    // seamlessly on the keyboard.
    if(IsKeyDown(BIND_JUMP) && player.onGround) {
        player.velocity.y = JUMP_VELOCITY;
        player.onGround   = false;
    }

    // Apply Ground Friction
    if(player.onGround) {
        float speed = sqrtf(player.velocity.x * player.velocity.x + player.velocity.z * player.velocity.z);
        if(speed > 0.1f) {
            float friction = GROUND_FRICTION;
            // Apply a minimum control speed to ensure we come to a full stop quickly rather than sliding asymptotically
            float control = (speed < PLAYER_SPEED) ? PLAYER_SPEED : speed;
            float drop = control * friction * dt; 

            float newSpeed = std::max(speed - drop, 0.0f);
            newSpeed /= speed; // Get scaling factor
            player.velocity.x *= newSpeed;
            player.velocity.z *= newSpeed;
        } else {
            player.velocity.x = 0;
            player.velocity.z = 0;
        }
    }

    // Accelerate (Air & Ground)
    float wishSpeed = maxSpeed;
    if(!player.onGround) {
        // Keep airborne steering intentionally limited for tactical movement.
        wishSpeed = std::min(wishSpeed, maxSpeed * AIR_CONTROL_RATIO);
    }

    float currentSpeed = player.velocity.x * wishDir.x + player.velocity.z * wishDir.z;
    float addSpeed = wishSpeed - currentSpeed;

    if(addSpeed > 0.0f) {
        float accel = player.onGround ? GROUND_ACCEL : AIR_ACCEL;
        float accelSpeed = accel * wishSpeed * dt;
        if(accelSpeed > addSpeed) accelSpeed = addSpeed;

        player.velocity.x += accelSpeed * wishDir.x;
        player.velocity.z += accelSpeed * wishDir.z;
    }

    // ── Gravity — runs every frame unconditionally ────────────────────────
    // This is what pulls the player back down after a jump.
    player.velocity.y += GRAVITY * dt;
    if(player.velocity.y < -50.0f) player.velocity.y = -50.0f;

    // ── Sweep ─────────────────────────────────────────────────────────────
    bool hitFloor = false;
    player.xform.pos = SweepAABB(player.xform.pos, player.velocity, dt, hitFloor, world.solids, player.height());

    if(hitFloor && player.velocity.y <= 0.0f) {
        player.velocity.y = 0.0f;
        player.onGround   = true;
    } else if(!hitFloor) {
        player.onGround   = false;
    }

    // ── Weapon select 1–5 & Scroll ────────────────────────────────────────
    for(int k = 0; k < (int)WeaponID::COUNT; k++) {
        if(IsKeyPressed(KEY_ONE + k)) {
            TrySwitchWeapon(player, (WeaponID)k);
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        int currentId = (int)player.weapon.id;
        int maxWeapons = (int)WeaponID::COUNT - 1; // max index

        if (wheel > 0.0f) {
            currentId = std::min(currentId + 1, maxWeapons);
        } else if (wheel < 0.0f) {
            currentId = std::max(currentId - 1, 0);
        }

        if (currentId != (int)player.weapon.id) {
            TrySwitchWeapon(player, (WeaponID)currentId);
        }
    }

    // ── ADS ───────────────────────────────────────────────────────────────
    player.weapon.isADS = IsMouseButtonDown(BTN_ADS);

    // ── Fire ──────────────────────────────────────────────────────────────
    bool triggerDown    = IsMouseButtonDown(BTN_FIRE);
    bool triggerPressed = IsMouseButtonPressed(BTN_FIRE);
    bool shouldFire     = player.weapon.stats().semiAuto ? triggerPressed : triggerDown;
    if(shouldFire) WeaponFire(player, world, player.weapon.isADS, &audio);

    // ── Reload ────────────────────────────────────────────────────────────
    if(IsKeyPressed(BIND_RELOAD) &&
       player.weapon.reloadTimer <= 0 &&
       player.weapon.ammoReserve > 0 &&
       player.weapon.ammoMag < player.weapon.stats().magSize)
        player.weapon.reloadTimer = player.weapon.stats().reloadTimeSec;

    // ── Weapon tick ───────────────────────────────────────────────────────
    WeaponTick(player.weapon, dt);

    // ── Utility ───────────────────────────────────────────────────────────
    if(IsKeyPressed(BIND_FRAG))  ThrowUtility(player, UtilityID::FRAG,  world);
    if(IsKeyPressed(BIND_SMOKE)) ThrowUtility(player, UtilityID::SMOKE, world);
    if(IsKeyPressed(BIND_STUN))  ThrowUtility(player, UtilityID::STUN,  world);
}