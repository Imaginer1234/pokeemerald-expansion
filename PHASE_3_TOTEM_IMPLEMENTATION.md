# Phase 3: Totem Pokémon System Implementation

**Objective:** Implement a custom battle mechanic where a designated "Totem" Pokémon:
1. Gets a stat boost when sent out (e.g., +1 Sp.Atk for Lurantis)
2. After N turns or when HP drops below a threshold, summons an ally Pokémon mid-battle (converting 1v1 to 2v1)

This is a **two-part implementation** split into simpler, testable chunks.

---

## Part 1: Totem Stat Boost (On Send-Out)

### Step 1.1: Create Totem Data Structure

**File:** Create `src/data/totem_battles.h`

```c
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
    {SPECIES_GUMSHOOS, STAT_ATK, 1, SPECIES_RATTATA, 10, 3, 0},
    
    // Akala Trial Gym (Lurantis as finale)
    {SPECIES_LURANTIS, STAT_SPATK, 1, SPECIES_COMFEY, 28, 4, 0},
    
    // Ula'ula Trial Gym (Mimikyu)
    {SPECIES_MIMIKYU, STAT_ATK, 1, SPECIES_MURKROW, 40, 5, 0},
    
    // Poni Trial Gym (Ribombee)
    {SPECIES_RIBOMBEE, STAT_SPATK, 1, SPECIES_ORICORIO, 50, 5, 0},
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
```

### Step 1.2: Add Totem Battle Flags to Battle Struct

**File:** `include/battle.h` (find `struct BattleStruct`)

Add these fields to the struct (paste after existing volatile flags):

```c
struct BattleStruct {
    // ... existing fields ...
    
    // Totem battle fields
    bool8 isTotemBattle;
    u16 totemBattlerMonSpecies[MAX_BATTLERS];  // Track which battler is a Totem
    u8 totemTurnCounter;                       // Turn count for ally summon trigger
    bool8 totemAllyAlreadySummoned;            // Prevent re-summoning
    
    // ... rest of struct ...
};
```

### Step 1.3: Hook Totem Stat Boost into Switch-In

**File:** `src/battle_util.c`

Find the `case ABILITYEFFECT_ON_SWITCHIN:` block (you already found it). After the giant switch statement inside it, before the closing brace of the case, add this:

```c
        // If no ability effect triggered, check for Totem boost
        if (effect == 0)
        {
            const struct TotemData *totemData = GetTotemData(gBattleMons[battler].species);
            if (totemData != NULL && gBattleStruct->isTotemBattle)
            {
                // Apply Totem stat boost
                gBattleScripting.battler = battler;
                gBattleScripting.bank = totemData->statToBoost;
                gBattleScripting.statChanger = totemData->boostStages;
                BattleScript_StatChangeProcess(battler, STAT_CHANGE_STATS, totemData->statToBoost, totemData->boostStages, FALSE);
                effect++;
            }
        }
```

At the top of `src/battle_util.c`, add the include:

```c
#include "data/totem_battles.h"
```

### Step 1.4: Test Part 1

Once compiled, create a test trainer battle (we'll do this in Phase 4, but document the test plan here):

- Spawn a Totem Lurantis (level 28) with `gBattleStruct->isTotemBattle = TRUE`
- Send your Pokémon out
- Lurantis should gain +1 Sp.Atk on switch-in (visible in battle UI stat stage indicator)

---

## Part 2: Mid-Battle Ally Summon (High Risk)

### ⚠️ WARNING: This is the riskiest part. Read before coding.

The GBA engine was not designed for mid-battle party swaps. The codebase does support dynamic double battles (trainer AI can switch mons), but **forcing a new ally to appear for the opponent mid-battle is a custom system** that doesn't have a native template.

**Fallback plan:** If this part proves too complex after a reasonable attempt (2-4 hours), revert to: "Totem fights are always double battles from turn 1" (both Totem + ally start on field). This is fully supported and still captures the spirit.

### Step 2.1: Add Turn Counter to Battle Loop

**File:** `src/battle_main.c`

Search for where `gBattleStruct->turns` is incremented (likely in the turn-execution loop). Find the line that increments the global turn counter and add:

```c
    if (gBattleStruct->isTotemBattle)
    {
        gBattleStruct->totemTurnCounter++;
        
        // Check if Totem ally should summon
        const struct TotemData *totemData = GetTotemData(gBattleMons[gBattlerAttacker].species);
        if (totemData != NULL 
            && gBattleStruct->totemTurnCounter >= totemData->turnThreshold
            && !gBattleStruct->totemAllyAlreadySummoned)
        {
            // Trigger ally summon logic (implemented in Step 2.2)
            BattleScript_TotemAllyAppears(gBattlerAttacker, totemData->allySpecies, totemData->allyLevel);
            gBattleStruct->totemAllyAlreadySummoned = TRUE;
        }
    }
```

### Step 2.2: Implement Ally Summon (Most Complex Part)

**File:** Create `src/data/battle_scripts_totem.s` (ASM) OR extend `src/battle_script_commands.c`

**Option A (Recommended):** Implement as a battle script command rather than ASM. This is slower but more maintainable.

In `src/battle_script_commands.c`, add a new command handler:

```c
void BattleScript_TotemAllyAppears(u8 totemBattler, u16 allySpecies, u8 allyLevel)
{
    // This is a placeholder — full implementation requires:
    // 1. Creating a new Pokémon in the opponent's party at runtime
    // 2. Swapping battle-field layout from 1v1 to 2v1 (double battle config)
    // 3. Running switch-in animations for the new ally
    // 4. Adjusting battle UI to show 2v1 state
    
    // CRITICAL: This requires deep knowledge of gBattleParties[], gBattleMons[],
    // double-battle layout structs, and battle field graphics setup.
    // NOT TRIVIAL — high risk of crashes if field layout isn't properly updated.
    
    // Pseudocode:
    // - Generate new battle mon from allySpecies/allyLevel
    // - Insert into opponent's party (shift existing mons if needed)
    // - Update gBattleFieldStatus to indicate double battle
    // - Call existing "switch in" animation routines
    // - Return control to battle main loop
}
```

**Option B (Fallback — STRONGLY RECOMMENDED for this project):**

Skip mid-battle summon entirely. Instead, **make all Totem fights double battles from the start**:

In Step 2.1, instead of calling `BattleScript_TotemAllyAppears`, set:

```c
gBattleStruct->isDoubleOpponentBattle = TRUE;  // or equivalent double-battle flag
// Pre-populate opponent's party with Totem + ally both at battle start
```

This is **fully supported** by the expansion and avoids the complexity of mid-battle field restructuring.

### Step 2.3: Documentation & Decision Log

**File:** `notes/totem_implementation.md`

Create/update with:

```markdown
# Totem Implementation Status

## Part 1: Stat Boost ✅
- Implemented in `src/battle_util.c`
- Data driven from `src/data/totem_battles.h`
- Status: COMPLETE

## Part 2: Mid-Battle Ally Summon
- Attempted: [YES/NO]
- Status: [BLOCKED / FALLBACK USED]
- Reason: Ally summon mid-battle requires field-state restructuring not natively supported in GBA engine.
- Decision: Using double-battle-from-start variant instead (ally already on field).
  This captures the spirit without the engine complexity.

## Fallback Used: Double Battles From Start
- Both Totem + ally spawn from turn 1
- No mid-battle party manipulation
- Clean implementation, fully tested pathway
```

---

## Integration Checklist

- [ ] `src/data/totem_battles.h` created with Totem data table
- [ ] `include/battle.h` updated with `isTotemBattle` flags
- [ ] `src/battle_util.c` includes `totem_battles.h` and hooks stat boost
- [ ] `src/battle_main.c` has turn-counter logic for ally summon trigger
- [ ] Fallback decision documented in `notes/totem_implementation.md`
- [ ] Build succeeds with no new compiler errors
- [ ] One test Totem battle created (Phase 4)

---

## Commit Strategy

```bash
git add src/data/totem_battles.h include/battle.h src/battle_util.c
git commit -m "feat: totem stat-boost system (part 1)"

git add src/battle_main.c notes/totem_implementation.md
git commit -m "feat: totem ally-summon with double-battle fallback (part 2)"
```

---

## Testing Plan (Phase 4)

Once the Akala maps are built in Porymap:
1. Create a test trainer with Totem Lurantis (level 28) as their only Pokémon
2. Place trainer on a test route
3. Battle them with a comparable-level Pokémon
4. Verify Lurantis gains +1 Sp.Atk on send-out
5. (If double-battle fallback used) Verify ally appears from turn 1, not mid-battle
6. Document results in `notes/test_results.md`
