# Data Models and Persistence

This document describes TurnBasedEQ's key data structures, SQLite schema, entity models, and persistence patterns.

See also: [shared.md](shared.md), [auth.md](auth.md), [content-and-data.md](content-and-data.md).

---

## Persistence strategy

TurnBasedEQ uses a **hybrid model**:

| Data | Storage |
|------|---------|
| Accounts, sessions | Relational SQLite tables |
| Character identity, zone, position | Relational `characters` table |
| Runtime state (inventory, skills, equipment, vitals) | JSON blob in `characters.state_json` |
| World layout | Relational zone tables + JSON tile arrays |

Access layer: `tbeq::db::Database` in `shared/include/tbeq/persistence/Database.hpp`.

---

## SQLite schema

Schema initialized by `Database::initializeSchema()` in `shared/src/persistence/Database.cpp`.

### Core tables

```sql
-- Accounts
accounts (
    id INTEGER PRIMARY KEY,
    username TEXT UNIQUE,
    password_hash TEXT,
    password_salt TEXT,
    is_dev INTEGER,
    is_banned INTEGER
)

-- Sessions (token hash as key)
sessions (
    token INTEGER PRIMARY KEY,
    account_id INTEGER REFERENCES accounts(id),
    expires_at INTEGER
)

-- Characters
characters (
    id TEXT PRIMARY KEY,
    account_id INTEGER REFERENCES accounts(id),
    name TEXT,
    race_id TEXT,
    class_id TEXT,
    level INTEGER,
    zone_id TEXT,
    pos_x REAL,
    pos_y REAL,
    state_json TEXT
)
```

### World tables

| Table | Purpose |
|-------|---------|
| `worlds` | World seed, version, generation timestamp |
| `zones` | Zone metadata (name, size, biome, safe flag) |
| `zone_tiles` | JSON tile grid per zone |
| `zone_portals` | Portal tile → destination zone/position |
| `zone_spawns` | Mob spawn points and respawn timers |
| `zone_npc_slots` | NPC placement (merchant, lorekeeper slots) |
| `zone_links` | Adjacency graph between zones |
| `zone_triggers` | Trigger volumes (future use) |

---

## Database record structs

From `Database.hpp`:

| Struct | Use |
|--------|-----|
| `AccountRecord` | Login lookup |
| `SessionRecord` | Token validation |
| `CharacterRecord` | Full character row including `stateJson` |
| `CharacterSummary` | Character select list |
| `ZoneMetadataRecord` | Zone load parameters |
| `ZonePortalRecord` | Portal definitions |
| `ZoneSpawnRecord` | Spawn point config |
| `NpcSlotRecord` | NPC slot placement |
| `ZoneLinkRecord` | Zone graph edges |

---

## CharacterState

**File:** `shared/include/tbeq/core/CharacterState.hpp`

Runtime character state serialized to/from `state_json`:

```cpp
struct CharacterState
{
    uint16_t hp, maxHp, mana, maxMana;
    uint16_t agi, cha;
    uint32_t gold;
    std::string classId;
    uint16_t level;
    std::unordered_map<std::string, SkillProgress> skills;
    std::vector<std::string> unlockedSpells;
    std::vector<std::string> unlockedAbilities;
    std::vector<InventoryItem> inventory;
    std::unordered_map<std::string, std::string> equipment; // slot → itemId
    std::string equippedWeaponSkill;
    bool godMode;
};
```

### Key methods

| Method | Purpose |
|--------|---------|
| `createDefault(classId, level)` | New character baseline |
| `toJson()` / `fromJson()` | Persistence serde |
| `addItem` / `removeItem` | Inventory mutations |
| `knowsSpell` / `knowsAbility` | Combat eligibility |
| `equippedItemInSlot(slot)` | Equipment lookup |

### SkillProgress

```cpp
struct SkillProgress
{
    uint16_t level = 1;
    uint32_t experience = 0;
};
```

---

## Server entity models

Defined in `server/zone/ZoneServer.hpp`:

### PlayerEntity

| Field | Type | Description |
|-------|------|-------------|
| `characterId`, `name` | string | Identity |
| `raceId`, `classId`, `level` | — | Character metadata |
| `characterState` | CharacterState | Live runtime state |
| `sessionTokenHash` | uint64_t | Auth |
| `tileX`, `tileY` | int32_t | Zone position |
| `entityId` | uint32_t | Network entity id |
| `connection` | TcpConnection | Client link |
| `inCombat`, `combatId`, `combatSlot` | — | Combat linkage |

### AiPartyMember

Same combat/position fields as player but no TCP connection. Spawned by debug command or server logic; follows leader.

### NpcEntity

| Field | Description |
|-------|-------------|
| `npcId` | Content catalog id |
| `slotType` | e.g. merchant, lore |
| `tileX`, `tileY` | Fixed position |
| `merchantStockRemaining` | Per-NPC buy stock depletion |

### ZoneSpawnState

Mob spawn with respawn timer (`respawnAtUnix`, `mobTable`, `active`).

---

## Zone grid model

**File:** `shared/include/tbeq/world/ZoneGrid.hpp`

Loaded from database per zone server:

| Property | Source |
|----------|--------|
| `width`, `height` | `zones` table |
| `tiles` | `zone_tiles.tile_data` JSON |
| `portals` | `zone_portals` rows |
| `isSafe` | Zone metadata (no aggro) |
| `tileStyle` | Rendering hint |

### Collision

`TileDefCatalog` maps tile keys to `TileCollision` (walkable, blocked, etc.).

Methods: `isWalkable`, `canStepTo`, `portalAt`, `inBounds`.

---

## Content definition models

### ItemDef

**File:** `shared/include/tbeq/content/ItemCatalog.hpp`

Slots: Weapon, Head, Chest, Hands. Stats block, class/race restrictions, vendor value, weapon skill linkage.

### MobDef

**File:** `shared/include/tbeq/content/MobCatalog.hpp`

Combat stats + loot table entries with drop chances.

### NpcDef

**File:** `shared/include/tbeq/content/NpcCatalog.hpp`

Roles (merchant, lore), dialog lines, buy/sell stock lists.

### SpellDef / AbilityDef

**Files:** `SpellCatalog.hpp`, `AbilityCatalog.hpp`

Mana cost, cast time, target rules, linked skill schools, effect values.

---

## Persistence call sites

| Event | Action |
|-------|--------|
| Player disconnect | `updateCharacterLocation` + `updateCharacterState` |
| Portal transfer (source) | Save before evict |
| Portal transfer (dest) | Load from DB on `PlayerEnterPrepare` |
| Combat end | `CombatManager` persists via callback |
| Equip/merchant | Immediate state save |

---

## Database configuration

Default path: `config/tbeq.db` (set in `AppConfig.dbPath`).

Pragmas (on open):

- `foreign_keys = ON`
- `journal_mode = WAL`
- `busy_timeout` for concurrent zone servers

`Database` is **move-only** (copy deleted) with RAII `sqlite3*` lifecycle.

---

## JSON state example (conceptual)

Character `state_json` stores inventory and equipment:

```json
{
  "hp": 100,
  "maxHp": 120,
  "mana": 50,
  "gold": 350,
  "skills": { "1h_slash": { "level": 5, "experience": 120 } },
  "inventory": [{ "itemId": "health_potion", "quantity": 3 }],
  "equipment": { "weapon": "rusty_sword", "chest": "leather_tunic" },
  "unlockedSpells": ["minor_heal"],
  "unlockedAbilities": ["bash"]
}
```

Exact schema is defined by `CharacterState::toJson()` implementation.

---

## Related documentation

- [auth.md](auth.md) — accounts and sessions
- [server.md](server.md) — when persistence is triggered
- [content-and-data.md](content-and-data.md) — JSON catalog schemas
- [architecture-overview.md](architecture-overview.md) — zone transfer state handoff
