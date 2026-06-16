#ifndef GUARD_TOTEM_BATTLES_H
#define GUARD_TOTEM_BATTLES_H

#include "constants/species.h"
#include "constants/battle.h"

// Totem metadata: which stat to boost, and the ally Pokémon
struct TotemData {
    u16 totemSpecies;
    u8 statToBoost;      // STAT_ATK, STAT_DEF, STAT_SPEED, STAT_SPATK, STAT_SPDEF, STAT_HP
    u8 boostStages;      // Number of stages (+1, +2, etc.)
    u16 allySpecies;     // Pokémon to summon as ally
    u8 allyLevel;        // Level of ally
    u8 turnThreshold;    // Number of turns before ally summons (or 0 if HP-based)
    u8 hpThreshold;      // If > 0, ally summons when Totem HP <= this % (e.g., 50 for 50%)
};

// Totem encounter table
static const struct TotemData gTotemBattles[] = {
    // Melemele Trial Gym (Gumshoos or Raticate — placeholder, update per design)
    {SPECIES_GUMSHOOS_TOTEM, STAT_ATK, 1, SPECIES_RATTATA, 10, 3, 0},

    // Akala Trial Gym (Lurantis as finale)
    {SPECIES_LURANTIS_TOTEM, STAT_SPATK, 1, SPECIES_COMFEY, 28, 4, 0},

    // Ula'ula Trial Gym (Mimikyu)
    {SPECIES_MIMIKYU_TOTEM_DISGUISED, STAT_ATK, 1, SPECIES_MURKROW, 40, 5, 0},

    // Poni Trial Gym (Ribombee)
    {SPECIES_RIBOMBEE_TOTEM, STAT_SPATK, 1, SPECIES_ORICORIO, 50, 5, 0},
};

#define TOTEM_ENCOUNTERS_COUNT ARRAY_COUNT(gTotemBattles)

// Helper function to look up Totem data by species
static inline const struct TotemData *GetTotemData(u16 species)
{
    for (u32 i = 0; i < TOTEM_ENCOUNTERS_COUNT; i++)
    {
        if (gTotemBattles[i].totemSpecies == species)
            return &gTotemBattles[i];
    }
    return NULL;
}

#endif // GUARD_TOTEM_BATTLES_H