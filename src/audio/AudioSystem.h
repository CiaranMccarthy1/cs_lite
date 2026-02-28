#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  AudioSystem.h  –  Minimal audio, sniper shot only
// ─────────────────────────────────────────────────────────────────────────────
#include <raylib.h>
#include "../Constants.h"

struct AudioSystem {

    Sound sniperShot = {};
    bool  loaded     = false;

    void Init() {
        if(FileExists("assets/audio/sniper.mp3")) {
            sniperShot = LoadSound("assets/audio/sniper.mp3");
            loaded     = true;
        } else {
            TraceLog(LOG_WARNING, "AudioSystem: assets/audio/sniper.mp3 not found");
        }
    }

    void Shutdown() {
        if(loaded) UnloadSound(sniperShot);
    }

    void PlayShoot(WeaponID id) {
        if(!loaded) return;
        if(id == WeaponID::SNIPER) {
            StopSound(sniperShot);
            PlaySound(sniperShot);
        }
    }
};