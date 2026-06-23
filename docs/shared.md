# Shared Library (`tbeq_shared`)

The shared library is the contract between client, servers, tools, and tests. All domain logic that must agree on wire format or game rules lives here.

**CMake target:** `tbeq_shared` (`shared/CMakeLists.txt`)  
**Include root:** `shared/include/tbeq/`

See also: [cpp-architecture.md](cpp-architecture.md), [module-reference.md](module-reference.md).

---

## Namespace map

| Namespace | Directory | Purpose |
|-----------|-----------|---------|
| `tbeq` | `core/` | Config, logging, `CharacterState` |
| `tbeq::net` | `net/` | Packets, serialization, debug commands |
| `tbeq::db` | `persistence/` | SQLite `Database` wrapper |
| `tbeq::auth` | `auth/` | Password hash, validation |
| `tbeq::combat` | `combat/` | Combat types and `CombatInstance` |
| `tbeq::content` | `content/` | JSON catalogs (items, mobs, spells, etc.) |
| `tbeq::items` | `items/` | Equip rules, merchant pricing |
| `tbeq::skills` | `skills/` | `SkillResolver`, skill progress |
| `tbeq::ai` | `ai/` | Combat AI brains and profiles |
| `tbeq::world` | `world/` | Tile defs, `ZoneGrid` |
| `tbeq::worldgen` | `worldgen/` | Procedural world generation |
| `tbeq::social` | `social/` | Chat channel, invites |
| `tbeq::npc` | `npc/` | Dialog trees, interaction resolver |

---

## Dependencies

Linked publicly by `tbeq_shared`:

| Library | Usage |
|---------|-------|
| spdlog | Structured logging |
| nlohmann_json | JSON catalogs, character state, world tiles |
| tbeq_asio | ASIO headers (standalone) |
| tbeq_sqlite3 | Persistence |

---

## Core modules

### Config and logging

| File | Description |
|------|-------------|
| `core/Config.hpp` | `AppConfig`, `parseServerArgs()` |
| `core/Log.hpp` | `initLogging(processName)` — file + console sinks |
| `core/CharacterState.hpp` | Runtime character state with JSON serde |

`CharacterState` holds HP/mana, skills, inventory, equipment, unlocked spells/abilities, gold, and derived combat stats.

### Networking

| File | Description |
|------|-------------|
| `net/PacketHeader.hpp` | 24-byte frame header, magic `TBEQ` |
| `net/PacketTypes.hpp` | `ClientPacketType`, `ServerPacketType` enums |
| `net/ClientPackets.hpp` | All client-visible payload structs |
| `net/PacketSerializer.hpp` | `ByteWriter`, `ByteReader`, encode/decode |
| `net/DebugCommands.hpp` | Dev command enum and payloads |

Serialization is manual (no protobuf). Round-trip tests in `tests/unit/net/packet_roundtrip_test.cpp`.

See [networking.md](networking.md).

### Persistence

| File | Description |
|------|-------------|
| `persistence/Database.hpp` | RAII SQLite wrapper, schema, queries |

Hybrid persistence: relational tables for accounts/characters/world; JSON blob for runtime character state.

See [data-models.md](data-models.md).

### Auth

| File | Description |
|------|-------------|
| `auth/PasswordHash.hpp` | Hash and verify |
| `auth/CharacterValidation.hpp` | Input validation |

See [auth.md](auth.md).

---

## Game domain modules

### Combat

| File | Description |
|------|-------------|
| `combat/CombatTypes.hpp` | Enums, `CombatParticipant`, `CombatEvent` |
| `combat/CombatInstance.hpp` | Turn-based combat state machine |

Pure domain logic with injected RNG. Used by server `CombatManager` and unit tests.

See [combat-system.md](combat-system.md).

### Skills

| File | Description |
|------|-------------|
| `skills/SkillDef.hpp` | Skill definition struct |
| `skills/SkillResolver.hpp` | Hit/damage/flee/spell formulas, XP caps |

Loads skill caps from class data; used in combat and meditation.

### Content catalogs

| Catalog | JSON file | Header |
|---------|-----------|--------|
| Items | `data/items.json` | `content/ItemCatalog.hpp` |
| Mobs | `data/mobs.json` | `content/MobCatalog.hpp` |
| Spells | `data/spells.json` | `content/SpellCatalog.hpp` |
| Abilities | `data/abilities.json` | `content/AbilityCatalog.hpp` |
| NPCs | `data/npcs.json` | `content/NpcCatalog.hpp` |

All expose `loadFromFile()` and `find*(id)` lookup.

### Items

| File | Description |
|------|-------------|
| `items/ItemRules.hpp` | Equip validation, stat recompute, merchant pricing |

### AI

| File | Description |
|------|-------------|
| `ai/ClassCombatProfile.hpp` | JSON-driven combat priorities |
| `ai/ClassCombatBrain.hpp` | Ability/spell selection for AI |
| `ai/PartyMemberAI.hpp` | AI companion turn logic |

Profiles loaded from `data/ai/class_combat_profiles.json`.

### World

| File | Description |
|------|-------------|
| `world/TileDefCatalog.hpp` | Tile collision and visual keys |
| `world/ZoneGrid.hpp` | Walkability, portals, tile queries |

### World generation

| File | Description |
|------|-------------|
| `worldgen/WorldGenerator.hpp` | Zone layout generation |
| `worldgen/WorldValidator.hpp` | Validation rules |
| `worldgen/WorldBootstrap.hpp` | `ensureWorldInDatabase()` |

### Social and NPC

| File | Description |
|------|-------------|
| `social/ChatChannel.hpp` | Say channel formatting |
| `social/Invite.hpp` | Party invite stub |
| `npc/DialogTree.hpp` | Lore dialog structure |
| `npc/InteractionResolver.hpp` | NPC interaction routing |

---

## Design principles

1. **Plain structs + catalogs** — minimal inheritance
2. **Non-owning catalog references** — lifetime at composition root
3. **Injected RNG** — deterministic combat tests
4. **enum class** — wire-safe domain enums
5. **std::optional** — DB lookups without sentinel values

Detailed C++ rationale: [cpp-architecture.md](cpp-architecture.md).

---

## Source file list

All `.cpp` files compiled into `tbeq_shared` (from `shared/CMakeLists.txt`):

```
src/core/Log.cpp, Config.cpp, CharacterState.cpp
src/net/PacketSerializer.cpp
src/persistence/Database.cpp
src/auth/PasswordHash.cpp, CharacterValidation.cpp
src/worldgen/WorldGenerator.cpp, WorldValidator.cpp, WorldBootstrap.cpp
src/world/TileDefCatalog.cpp, ZoneGrid.cpp
src/content/*.cpp (5 catalogs)
src/items/ItemRules.cpp
src/combat/CombatInstance.cpp
src/ai/*.cpp (3 files)
src/skills/SkillResolver.cpp
src/social/ChatChannel.cpp, Invite.cpp
src/npc/DialogTree.cpp, InteractionResolver.cpp
```

---

## Related documentation

- [data-models.md](data-models.md) — key structs and DB schema
- [content-and-data.md](content-and-data.md) — JSON data files
- [module-reference.md](module-reference.md) — per-file reference
