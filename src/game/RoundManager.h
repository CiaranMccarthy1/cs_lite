#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  RoundManager.h  –  Round lifecycle: spawn, timer, win-condition, reset
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include "../ai/BotAI.h"
#include "../game/MapLoader.h"
#include <algorithm>
#include <array>

inline bool SpawnCollides(const Pawn& pawn, Vector3 spawnPos,
                          const std::vector<MapSolid>& solids) {
  float r = PLAYER_RADIUS;
  float h = pawn.height();
  BoundingBox box = {
      {spawnPos.x - r, spawnPos.y, spawnPos.z - r},
      {spawnPos.x + r, spawnPos.y + h, spawnPos.z + r}};
  for (const auto& s : solids) {
    if (CheckCollisionBoxes(box, s.bounds)) {
      return true;
    }
  }
  return false;
}

inline Vector3 ResolveSafeSpawn(const Pawn& pawn, Vector3 preferred,
                                const std::vector<MapSolid>& solids) {
  static constexpr std::array<Vector3, 9> OFFSETS = {{
      {0.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f},
      {-1.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, -1.0f},
      {1.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, -1.0f},
      {-1.0f, 0.0f, 1.0f},
      {-1.0f, 0.0f, -1.0f},
  }};

  for (const Vector3& o : OFFSETS) {
    Vector3 candidate = {preferred.x + o.x, preferred.y, preferred.z + o.z};
    if (!SpawnCollides(pawn, candidate, solids)) {
      return candidate;
    }
  }
  return preferred;
}

// ─── Initialise all 6 pawns from spawn data
// ───────────────────────────────────
inline void SpawnPawns(World &world, const MapData &md) {
  world.playerID = std::clamp(world.playerID, 0, MAX_PAWNS - 1);
  int attackIdx = 0, defendIdx = 0;

  // Collect spawn points per team
  std::vector<SpawnPoint> atkSpawns, defSpawns;
  for (auto &sp : md.spawns) {
    if (sp.team == Team::ATTACK)
      atkSpawns.push_back(sp);
    else
      defSpawns.push_back(sp);
  }

  for (int i = 0; i < MAX_PAWNS; i++) {
    Pawn &p = world.pawns[i];
    p.id = i;
    p.alive = true;
    p.hp = MAX_HP;
    p.fragCount = 1;
    p.smokeCount = 1;
    p.stunCount = 1;
    p.velocity = {0, 0, 0};
    p.onGround = true;
    p.isCrouching = false;

    // Player is pawn 0 on attack team
    p.isBot = (i != world.playerID);
    if (md.isTestMap && p.isBot) {
        p.alive = false;
    }
    p.team = (i < TEAM_SIZE) ? Team::ATTACK : Team::DEFEND;

    // Initialize all weapon slots so switching never creates ammo.
    for (int wid = 0; wid < (int)WeaponID::COUNT; wid++) {
      WeaponState ws;
      ws.id = (WeaponID)wid;
      ws.ammoMag = WEAPON_TABLE[wid].magSize;
      ws.ammoReserve = WEAPON_TABLE[wid].magSize * 3;
      ws.reloadTimer = 0.0f;
      ws.fireCooldown = 0.0f;
      ws.isADS = false;
      ws.shotsFired = 0;
      ws.timeSinceLastShot = 0.5f;
      p.weaponSlots[wid] = ws;
    }

    WeaponID startWeapon = (p.team == Team::ATTACK) ? WeaponID::RIFLE : WeaponID::SMG;
    p.weapon = p.weaponSlots[(int)startWeapon];

    // Position from spawn list
    std::vector<SpawnPoint> &spList =
        (p.team == Team::ATTACK) ? atkSpawns : defSpawns;
    int &idx = (p.team == Team::ATTACK) ? attackIdx : defendIdx;
    if (!spList.empty()) {
      auto &sp = spList[idx % spList.size()];
      Vector3 spawnPos = {sp.pos.x, sp.pos.y + 0.1f, sp.pos.z};

      // Spread extra teammates if the map provides fewer spawns than team size.
      if ((int)spList.size() < TEAM_SIZE) {
        int lane = (idx % TEAM_SIZE) - 1;
        float push = 0.8f * (float)(idx / std::max(1, (int)spList.size()));
        spawnPos.x += lane * 0.75f;
        spawnPos.z += (p.team == Team::ATTACK ? -push : push);
      }

      p.xform.pos = ResolveSafeSpawn(p, spawnPos, world.solids);
      p.xform.yaw = sp.yaw;
      p.xform.pitch = 0;
      idx++;
    } else {
      // Deterministic fallback so pawns never keep stale positions.
      float laneOffset = (float)((idx % TEAM_SIZE) - 1) * 2.0f;
      if (p.team == Team::ATTACK) {
        p.xform.pos = {-12.0f + laneOffset, 0.1f, -12.0f};
        p.xform.yaw = 0.0f;
      } else {
        p.xform.pos = {12.0f + laneOffset, 0.1f, 12.0f};
        p.xform.yaw = (float)PI;
      }
      p.xform.pitch = 0.0f;
      p.xform.pos = ResolveSafeSpawn(p, p.xform.pos, world.solids);
      idx++;
    }
  }
}

// ─── Full round reset
// ─────────────────────────────────────────────────────────
inline void ResetRound(World &world, const MapData &md) {
  if (world.grenades.capacity() < MAX_GRENADES)
    world.grenades.reserve(MAX_GRENADES);
  if (world.smokes.capacity() < MAX_SMOKES)
    world.smokes.reserve(MAX_SMOKES);
  if (world.tracers.capacity() < MAX_TRACERS)
    world.tracers.reserve(MAX_TRACERS);
  if (world.impacts.capacity() < MAX_IMPACTS)
    world.impacts.reserve(MAX_IMPACTS);

  world.grenades.clear();
  world.smokes.clear();
  world.tracers.clear();
  world.impacts.clear();
  world.stun.timeLeft = 0;
  world.hitIndicatorAlpha = 0;
  world.objective.captureProgress = 0;
  world.objective.captured = false;
  world.roundTimer = ROUND_TIME_SEC;
  world.roundState = RoundState::WAITING;
  world.freezeTimer = 3.0f;
  world.roundOverTimer = 4.0f;
  world.roundWinner = Team::NONE;
  SpawnPawns(world, md);
  InitBotBrains(world);
}

// ─── Per-frame round logic
// ────────────────────────────────────────────────────
inline void UpdateRound(World &world, const MapData &md, float dt) {
  switch (world.roundState) {

  case RoundState::WAITING:
    world.freezeTimer -= dt;
    if (world.freezeTimer <= 0)
      world.roundState = RoundState::ACTIVE;
    break;

  case RoundState::ACTIVE: {
    world.roundTimer -= dt;

    // ── Objective capture ─────────────────────────────────────────────
    bool anyAttackerInZone = false;
    for (auto &p : world.pawns) {
      if (!p.alive || p.team != Team::ATTACK)
        continue;
      Vector3 toObj = Vector3Subtract(p.xform.pos, world.objective.pos);
      toObj.y = 0.0f;
      float d = Vector3Length(toObj);
      if (d < world.objective.radius) {
        anyAttackerInZone = true;
        break;
      }
    }
    if (anyAttackerInZone) {
      world.objective.captureProgress += dt;
      if (world.objective.captureProgress >= OBJECTIVE_CAPTURE_SEC) {
        world.objective.captured = true;
        world.roundWinner = Team::ATTACK;
        world.roundState = RoundState::ROUND_OVER;
        world.roundOverTimer = 4.0f;
        world.scoreAttack++;
        return;
      }
    } else {
      world.objective.captureProgress =
          std::max(0.0f, world.objective.captureProgress - dt * 0.5f);
    }

    // ── Win conditions ────────────────────────────────────────────────
    if (!md.isTestMap) {
        bool attackAlive = world.alivePawnsOnTeam(Team::ATTACK);
        bool defendAlive = world.alivePawnsOnTeam(Team::DEFEND);

        if (!attackAlive || world.roundTimer <= 0) {
          world.roundWinner = Team::DEFEND;
          world.roundState = RoundState::ROUND_OVER;
          world.roundOverTimer = 4.0f;
          world.scoreDefend++;
        } else if (!defendAlive) {
          world.roundWinner = Team::ATTACK;
          world.roundState = RoundState::ROUND_OVER;
          world.roundOverTimer = 4.0f;
          world.scoreAttack++;
        }
    }
    break;
  }

  case RoundState::ROUND_OVER: {
    // Auto-advance after 4 seconds
    world.roundOverTimer -= dt;
    if (world.roundOverTimer <= 0) {
      world.roundOverTimer = 4.0f;
      world.roundNumber++;
      if (world.scoreAttack >= 5 || world.scoreDefend >= 5) {
        world.roundState = RoundState::MATCH_OVER;
      } else {
        ResetRound(world, md);
      }
    }
    break;
  }

  case RoundState::MATCH_OVER:
    // GUI layer handles replay.
    break;
  }
}