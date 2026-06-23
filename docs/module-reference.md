# Module Reference

Per-module reference of TurnBasedEQ source files, primary types, and responsibilities. For architectural context see [architecture-overview.md](architecture-overview.md) and [shared.md](shared.md).

---

## Root

| File | Description |
|------|-------------|
| `CMakeLists.txt` | Project root, FetchContent deps, subdirectories |
| `README.md` | Quick start, phase features, layout summary |
| `vcpkg.json` | vcpkg manifest (imgui) |

---

## shared/ — tbeq_shared

### include/tbeq/core/

| File | Types | Description |
|------|-------|-------------|
| `Config.hpp` | `AppConfig`, `parseServerArgs` | CLI and defaults |
| `Log.hpp` | `initLogging` | spdlog file + console setup |
| `CharacterState.hpp` | `CharacterState`, `InventoryItem` | Runtime player state + JSON |

| Source | Description |
|--------|-------------|
| `src/core/Config.cpp` | Argument parsing |
| `src/core/Log.cpp` | Log sinks per process |
| `src/core/CharacterState.cpp` | Default state, inventory ops, JSON |

### include/tbeq/net/

| File | Types | Description |
|------|-------|-------------|
| `PacketHeader.hpp` | `PacketHeader`, constants | 24-byte frame |
| `PacketTypes.hpp` | `ClientPacketType`, `ServerPacketType` | Wire enums |
| `ClientPackets.hpp` | 40+ payload structs | All packet bodies |
| `PacketSerializer.hpp` | `ByteWriter`, `ByteReader`, `SerializedPacket` | Serde |
| `DebugCommands.hpp` | `DebugCommand`, request/response | Dev cheats |

| Source | Description |
|--------|-------------|
| `src/net/PacketSerializer.cpp` | encode/decode implementations |

### include/tbeq/persistence/

| File | Types | Description |
|------|-------|-------------|
| `Database.hpp` | `Database`, record structs | SQLite RAII |

| Source | Description |
|--------|-------------|
| `src/persistence/Database.cpp` | Schema, CRUD, world inserts |

### include/tbeq/auth/

| File | Types | Description |
|------|-------|-------------|
| `PasswordHash.hpp` | `PasswordHashResult`, hash/verify | Credentials |
| `CharacterValidation.hpp` | `ValidationResult`, validators | Input rules |

| Source | Description |
|--------|-------------|
| `src/auth/PasswordHash.cpp` | FNV-1a iterative hash |
| `src/auth/CharacterValidation.cpp` | Username/password/name rules |

### include/tbeq/combat/

| File | Types | Description |
|------|-------|-------------|
| `CombatTypes.hpp` | Enums, `CombatParticipant`, `CombatEvent` | Domain types |
| `CombatInstance.hpp` | `CombatInstance`, `CombatLootRoll` | Combat state machine |

| Source | Description |
|--------|-------------|
| `src/combat/CombatInstance.cpp` | Turn resolution, ~900 lines |

### include/tbeq/content/

| File | Types | Description |
|------|-------|-------------|
| `ItemCatalog.hpp` | `ItemDef`, `ItemCatalog` | Equipment/items |
| `MobCatalog.hpp` | `MobDef`, `MobCatalog` | Enemies + tables |
| `SpellCatalog.hpp` | `SpellDef`, `SpellCatalog` | Spells |
| `AbilityCatalog.hpp` | `AbilityDef`, `AbilityCatalog` | Class abilities |
| `NpcCatalog.hpp` | `NpcDef`, `NpcCatalog` | NPCs + merchants |

| Source | Description |
|--------|-------------|
| `src/content/*.cpp` | JSON loaders (5 files) |

### include/tbeq/items/

| File | Types | Description |
|------|-------|-------------|
| `ItemRules.hpp` | `ItemRules`, `EquipCheckResult` | Equip + merchant math |

| Source | Description |
|--------|-------------|
| `src/items/ItemRules.cpp` | Stat recompute, pricing |

### include/tbeq/skills/

| File | Types | Description |
|------|-------|-------------|
| `SkillDef.hpp` | Skill metadata | |
| `SkillResolver.hpp` | `SkillResolver`, `SkillProgress` | Formulas + XP |

| Source | Description |
|--------|-------------|
| `src/skills/SkillResolver.cpp` | Hit/damage/flee/spell math |

### include/tbeq/ai/

| File | Types | Description |
|------|-------|-------------|
| `ClassCombatProfile.hpp` | Profile catalog | JSON AI config |
| `ClassCombatBrain.hpp` | `ClassCombatBrain` | Enemy AI |
| `PartyMemberAI.hpp` | `PartyMemberAI` | Companion AI |

| Source | Description |
|--------|-------------|
| `src/ai/*.cpp` | 3 implementation files |

### include/tbeq/world/

| File | Types | Description |
|------|-------|-------------|
| `TileDefCatalog.hpp` | `TileDef`, collision enum | Tile metadata |
| `ZoneGrid.hpp` | `ZoneGrid`, `ZonePortal` | Zone collision + portals |

| Source | Description |
|--------|-------------|
| `src/world/TileDefCatalog.cpp` | Load tile defs |
| `src/world/ZoneGrid.cpp` | DB load, walkability |

### include/tbeq/worldgen/

| File | Types | Description |
|------|-------|-------------|
| `WorldBootstrap.hpp` | `ensureWorldInDatabase` | First-run setup |
| `WorldGenerator.hpp` | Generator class | Zone creation |
| `WorldValidator.hpp` | Validator | Graph checks |

| Source | Description |
|--------|-------------|
| `src/worldgen/*.cpp` | 3 implementation files |

### include/tbeq/social/ + npc/

| File | Description |
|------|-------------|
| `social/ChatChannel.hpp` | Say message formatting |
| `social/Invite.hpp` | Party invite stub |
| `npc/DialogTree.hpp` | Dialog node structure |
| `npc/InteractionResolver.hpp` | Route NPC interact by role |

---

## server/

### server/common/

| File | Types | Description |
|------|-------|-------------|
| `net/TcpConnection.hpp` | `TcpConnection`, `TcpAcceptor` | ASIO TCP |
| `debug/DebugCommandHandler.hpp` | `DebugCommandHandler` | Dev command dispatch |

| Source | Description |
|--------|-------------|
| `net/TcpConnection.cpp` | Async read/write |
| `debug/DebugCommandHandler.cpp` | Command routing |

### server/world_login/

| File | Types | Description |
|------|-------|-------------|
| `WorldLoginServer.hpp` | `WorldLoginServer` | Auth + broker |
| `main.cpp` | | Entry point |

| Source | Description |
|--------|-------------|
| `WorldLoginServer.cpp` | ~850 lines, all login/transfer handlers |

### server/zone/

| File | Types | Description |
|------|-------|-------------|
| `ZoneServer.hpp` | `ZoneServer`, entity structs | Zone simulation |
| `main.cpp` | | Entry point |

| Source | Description |
|--------|-------------|
| `ZoneServer.cpp` | ~1600 lines, packet handlers |
| `ZoneServerInventory.cpp` | Equip, merchant, inventory |
| `combat/CombatManager.hpp` | `CombatManager` | Combat orchestration |
| `combat/CombatManager.cpp` | ~850 lines | Timers, broadcast, loot |

---

## client/

### client/src/

| File | Description |
|------|-------------|
| `main.cpp` | SDL/ImGui loop, login UI, zone gameplay (~1400 lines) |

### client/net/

| File | Types | Description |
|------|-------|-------------|
| `ZoneClient.hpp` | `ZoneClient` | Gameplay TCP client |
| `LocalClusterLauncher.hpp` | Cluster spawner | Dev convenience |
| `LoginProfiler.hpp` | Login timing | Diagnostics |

### client/render/

| File | Description |
|------|-------------|
| `TilemapRenderer.cpp` | Tile grid rendering |
| `EntityRenderer.cpp` | Entity sprites + nameplates |
| `SpriteRenderer.cpp` | Blit helpers |
| `TextureCache.cpp` | SDL texture cache |
| `procedural/TileGenerator.cpp` | Procedural tiles |
| `procedural/SpriteGenerator.cpp` | Procedural entity sprites |
| `procedural/ItemIconGenerator.cpp` | Item icons |
| `AnimationTypes.hpp` | Animation enums |

### client/ui/

| File | Description |
|------|-------------|
| `GameWindow.cpp` | HUD + chat shell |
| `WindowLayoutManager.cpp` | Layout persistence |
| `combat/CombatWindow.cpp` | Combat UI + ability bar |
| `inventory/InventoryWindow.cpp` | Inventory + equip |
| `look/LookWindow.cpp` | Character inspect |
| `merchant/MerchantWindow.cpp` | Buy/sell UI |
| `dialog/NpcDialogWindow.cpp` | Lorekeeper dialog |
| `debug/DebugMenu.cpp` | F1 debug panel |
| `debug/LogViewer.cpp` | Live log tail |
| `debug/UnitTestRunner.cpp` | In-client test runner |

---

## tools/

| Target | File | Description |
|--------|------|-------------|
| `tbeq_data_validator` | `data_validator/main.cpp` | JSON validation |
| `tbeq_worldgen` | `worldgen/main.cpp` | World gen CLI |

---

## tests/

| Path | Description |
|------|-------------|
| `helpers/TestCluster.*` | Process spawning |
| `helpers/HeadlessClient.*` | Protocol test client |
| `helpers/TempDatabase.*` | Isolated DB |
| `unit/**/*.cpp` | 16 unit test files |
| `integration/*.cpp` | 7 integration test files |

---

## scripts/

| Script | Description |
|--------|-------------|
| `bootstrap_third_party.ps1` | Extract vendored deps |
| `run_cluster.ps1` | Multi-zone dev cluster launcher |

---

## config/ and data/

| Path | Description |
|------|-------------|
| `config/tbeq.db` | Runtime SQLite (generated) |
| `config/ui_layout.json` | Client window layout |
| `data/*.json` | Game content — see [content-and-data.md](content-and-data.md) |

---

## Namespace → directory quick map

| Namespace | Primary directory |
|-----------|-------------------|
| `tbeq::net` | `shared/include/tbeq/net/` |
| `tbeq::combat` | `shared/include/tbeq/combat/` |
| `tbeq::db` | `shared/include/tbeq/persistence/` |
| `tbeq::server` | `server/world_login/`, `server/zone/` |
| `tbeq::client` | `client/net/` |
| `tbeq::content` | `shared/include/tbeq/content/` |
| `tbeq::test` | `tests/helpers/` |

---

## Related documentation

- [DOCUMENTATION.md](DOCUMENTATION.md) — full doc index
- [cpp-architecture.md](cpp-architecture.md) — C++ patterns by file
- [build-and-run.md](build-and-run.md) — build targets listed here
