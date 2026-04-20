#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  MapLoader.h  –  Parse the text-based map format into World geometry
//
//  MAP FILE FORMAT (assets/maps/map01.map):
//
//  # Lines beginning with '#' are comments
//
//  # SOLID  minX minY minZ  maxX maxY maxZ  R G B  [floor]
//  SOLID  -20  0  -20   20  0.1  20   60 60 60  floor
//  SOLID   -20  0  -20  -19.5  4  20   90 90 100
//
//  # WAYPOINT  id  x  y  z
//  WAYPOINT  0   0   0   0
//
//  # EDGE  fromID  toID   (bidirectional)
//  EDGE   0  1
//
//  # OBJECTIVE  x  y  z  radius
//  OBJECTIVE  5  0  5  3
//
//  # SPAWN  team(0=attack,1=defend)  x  y  z  yaw_deg
//  SPAWN  0   -10  0  0   0
// ─────────────────────────────────────────────────────────────────────────────
#include "../World.h"
#include <raylib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cmath>

struct SpawnPoint {
    Team    team;
    Vector3 pos;
    float   yaw; // radians
};

struct MapData {
    bool isTestMap = false;
    std::vector<SpawnPoint> spawns;
};

inline MapData LoadMap(const std::string& path, World& world) {
    world.solids.clear();
    world.waypoints.clear();

    std::ifstream f(path);
    if(!f.is_open())
        throw std::runtime_error("Cannot open map: " + path);

    MapData md;
    std::string line;

    while(std::getline(f, line)) {
        if(line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if(token == "TESTMAP") {
            md.isTestMap = true;
        }
        else if(token == "SOLID") {
            MapSolid s;
            float minX,minY,minZ,maxX,maxY,maxZ;
            int r,g,b;
            std::string floorTag;
            ss >> minX >> minY >> minZ >> maxX >> maxY >> maxZ >> r >> g >> b;
            ss >> floorTag;
            s.bounds = { {minX,minY,minZ}, {maxX,maxY,maxZ} };
            s.col    = { (unsigned char)r,(unsigned char)g,(unsigned char)b, 255 };
            s.isFloor = (floorTag == "floor");
            if((int)world.solids.size() < MAX_SOLIDS) {
                world.solids.push_back(s);
            } else {
                TraceLog(LOG_WARNING, "MapLoader: solid limit reached (%d)", MAX_SOLIDS);
            }
        }
        else if(token == "WAYPOINT") {
            int id; float x,y,z;
            ss >> id >> x >> y >> z;
            if(id < 0) {
                TraceLog(LOG_WARNING, "MapLoader: negative waypoint id ignored: %d", id);
                continue;
            }
            if(id >= MAX_WAYPOINTS) {
                TraceLog(LOG_WARNING, "MapLoader: waypoint id exceeds MAX_WAYPOINTS: %d", id);
                continue;
            }
            if((int)world.waypoints.size() <= id)
                world.waypoints.resize(id+1);
            world.waypoints[id].pos = {x,y,z};
        }
        else if(token == "EDGE") {
            int a,b; ss >> a >> b;
            if(a >= 0 && b >= 0 &&
               a < (int)world.waypoints.size() && b < (int)world.waypoints.size()) {
                world.waypoints[a].neighbours.push_back(b);
                world.waypoints[b].neighbours.push_back(a);
            }
        }
        else if(token == "OBJECTIVE") {
            float x,y,z,r; ss >> x >> y >> z >> r;
            world.objective.pos    = {x,y,z};
            world.objective.radius = std::max(0.5f, r);
        }
        else if(token == "SPAWN") {
            int t; float x,y,z,yawDeg;
            ss >> t >> x >> y >> z >> yawDeg;
            if(t != (int)Team::ATTACK && t != (int)Team::DEFEND) {
                TraceLog(LOG_WARNING, "MapLoader: invalid spawn team id ignored: %d", t);
                continue;
            }
            md.spawns.push_back({ (Team)t, {x,y,z}, yawDeg * DEG2RAD });
        }
    }
    return md;
}
