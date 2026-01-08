#pragma once
#include "plugin.h"

class DamageHook {
public:
    static void Install();
    static bool IsInstalled();

private:
    static bool TryInstallAtAddress(unsigned int addr);

    // NOTE: CPed::InflictDamage signature (per plugin-sdk):
    // bool InflictDamage(CEntity* damagedBy, eWeaponType type, float damage, ePedPieceTypes pedPiece, unsigned char direction);
    static bool __fastcall InflictDamageHook(
        CPed* self,
        void* /*edx*/,
        CEntity* damagedBy,
        eWeaponType weapon,
        float damage,
        ePedPieceTypes piece,
        unsigned char direction
    );

private:
    static bool s_installed;
    static unsigned int s_hookedAddr;

    // Trampoline points to copied original bytes + jump back
    using InflictDamage_t = bool(__thiscall*)(CPed*, CEntity*, eWeaponType, float, ePedPieceTypes, unsigned char);
    static InflictDamage_t s_original;
};
