#pragma once
// Pure function encoding the SA-accurate territory ownership rule on player death.
// No game engine dependencies — safe to include in unit test projects.
//
// SA rule (from wiki):
//   Dying before completing wave 1 (currentWave == 0) → territory reverts to the
//   defending gang (they held it off).
//   Dying after completing at least one wave (currentWave >= 1) → territory becomes
//   neutral/contested (-1); neither side fully owns it.

inline int ComputeWaveDeathOwner(int currentWave, int defendingGang) {
    return (currentWave >= 1) ? -1 : defendingGang;
}
