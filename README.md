# TurnBasedEQ

Greenfield C++ online MMO foundation (Phase 0): distributed server cluster, SDL2 + ImGui client, JSON-driven content, Catch2 tests, and CI.

## Prerequisites (Windows)

- Visual Studio 2022 with C++ desktop development
- CMake 3.24+
- [vcpkg](https://github.com/microsoft/vcpkg) (manifest mode; imgui is fetched via vcpkg)
- PowerShell

## Bootstrap dependencies

Most libraries are built from vendored source archives in `third_party/cache/` (asio, SDL2, spdlog, Catch2, sqlite, nlohmann-json). ImGui core library comes from vcpkg; SDL2 backend sources live in `third_party/imgui-src/backends/`.

```powershell
cd i:\TurnBasedEQ
.\scripts\bootstrap_third_party.ps1
```

## Build

### Fast builds (recommended)

Use **CMake presets** with the **Ninja** generator, parallel compiles (`/MP`), and optional **sccache**. From repo root:

```powershell
# One-time: set vcpkg location (adjust if yours differs)
$env:VCPKG_ROOT = "C:\vcpkg"

# Optional but recommended: install sccache for much faster rebuilds
# scoop install sccache   OR   choco install sccache

# Fastest iteration — client only, no tests (configure once, then rebuild)
cmake --preset ninja-debug-client
cmake --build --preset debug-client

# Or use the helper script (loads VS dev tools automatically):
.\scripts\build-fast.ps1 -Target client
.\scripts\build-fast.ps1 -Target client -Configure   # force reconfigure
.\scripts\build-fast.ps1 -Target tests -Preset debug-tests
.\scripts\build-fast.ps1 -Target release -Preset release-ci
```

| Preset | Use when |
|--------|----------|
| `ninja-debug-client` | Day-to-day client work (fastest configure) |
| `ninja-debug-tests` | Before pushing; includes Catch2 tests |
| `ninja-relwithdebinfo-client` | Faster compiles than Debug, still debuggable |
| `ninja-release-ci` | Matches CI Release build |
| `vs-debug-client` | Fallback if Ninja is unavailable |

Client binary with Ninja presets: `build/ninja-debug-client/client/tbeq_client.exe`

Run tests after a `ninja-debug-tests` configure:

```powershell
ctest --preset debug-tests
```

### One-command dev loop

Build (client + servers), start the cluster, launch the client, and stop servers when the client exits:

```powershell
.\scripts\run-dev.ps1
.\scripts\run-dev.ps1 -Configure          # first time or after CMake changes
.\scripts\run-dev.ps1 -SkipBuild        # skip rebuild if binaries are current
.\scripts\run-dev.ps1 -Zones starter_city # single zone only
```

### Legacy build (Visual Studio generator)

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Debug `
  -DTBEQ_BUILD_TESTS=ON

cmake --build build --config Debug --parallel
```

Release builds disable the client debug menu by default (`TBEQ_ENABLE_DEBUG_MENU=OFF`).

## Run tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Includes unit tests, integration tests, and CTest target `content_validation`.

### Phase 4 (dev mode)

- **Combat UI** — class ability bar (Warrior Bash/Kick, Cleric heal, Wizard nuke, Rogue backstab) plus mana display
- **Debug cheats** — Spawn AI Cleric, Fill mana, Unlock all spells (`F1` → Cheats tab)
- **Meditate** — out-of-combat mana regen via server (future client bind; server handles `MeditateRequest`)

### Phase 5 — Items, inventory, look, merchants

- **Equipment** — weapon, head, chest, hands slots with stat aggregation and class restrictions
- **Inventory UI** — `I` toggles draggable inventory window; equip/unequip via server packets
- **Look UI** — `L` toggles character inspect panel with equipped gear icons and stat bonuses
- **Procedural look** — head, chest, hands, and weapon tints applied to player sprites
- **Merchants** — gold-outlined NPCs in Starter City; `N` to interact; buy/sell with Merchant skill pricing
- **Merchant stock** — buy stock depletes per NPC until restock (zone restart)
- **Lorekeepers** — blue-outlined NPCs share lore lines via system chat on interact
- **Debug menu** — hidden by default in Debug builds; bottom-right `F1: Debug Menu` hint

### Phase 5.5 — Multi-zone cluster, navigation, persistence

- **Multi-zone dev cluster** — `run_cluster.ps1` starts WorldLogin plus all three generated zones (separate console window per process)
- **Portal navigation** — walk to a portal pad and press `P` to travel between Starter City, Hunting Grounds, and Goblin Cave
- **State handoff** — full character state (`state_json`, zone, tile) saved to shared SQLite on disconnect and before zone transfer; restored on login via WorldLogin broker + `LoadCharacter`
- **Per-zone logging** — `logs/zone_starter_city.log`, `logs/zone_starter_hunting.log`, etc.

### Phase 5.6 — Per-zone procedural generation

- **World skeleton** — world bootstrap writes `worlds`, zone stubs (`zone_type`, dimensions, biome), and `zone_links` (portal graph) only
- **Lazy zone generation** — each zone server generates tiles, portals, spawns, and NPC slots on first launch if `zone_tiles` is empty
- **Zone templates** — `data/worldgen/zone_templates.json` drives dimensions and entity counts per `zone_type` (`city`, `hunting`, `dungeon`)
- **Deterministic portals** — `PortalPlacementResolver` pairs cross-zone destination coordinates from `zone_links` and world seed
- **Dev regen** — delete a zone’s `zone_tiles` row (or `clearWorld` + restart) to regenerate that zone; two processes must not share the same `--zone-id`

### Phase 6 — Leveling, skills, enemies, and bosses

- **Character XP** — combat victory grants XP from mob level/HP; named and boss mobs pay bonus XP; level-up restores HP/mana and raises skill caps via `data/skill_caps.json`
- **Skill catalog** — 45 skills across melee, combat fundamentals/maneuvers, casting, stealth, exploration, crafting, and trade in `data/skills.json`
- **Skill progression** — combat swings, defend, taunt, meditate, and debug practice activities grant skill XP with level-scaled thresholds; `SkillGain` notifications in chat and Skills UI
- **Skills UI** — `K` toggles draggable Skills window with category sort, level/cap display, and recent skill-up highlights
- **Zone tiers & named mobs** — hunting tiers include **Grunt the Boar**; goblin tier adds **Skrit Nails**; Goblin Cave boss room spawns **Goblin Chief**
- **Boss phase script** — Goblin Chief enrages below 50% HP, switches to highest-threat targeting (Taunt matters), and hits harder
- **Defense & Taunt** — Defend mitigates damage using Defense skill; Warrior **Taunt** ability draws boss threat
- **Debug cheats** — set skill level, max skills, grant XP, practice forage/pick lock (`F1` → Cheats)
- **RaidCombatInstance stub** — header-only subclass for Phase 10 raid scale

## Run local server cluster

```powershell
.\scripts\run_cluster.ps1 -BuildConfig Debug
```

Starts **four processes** (WorldLogin + three zone servers) with `--dev-mode`. Each zone runs in its own console window for debug logs. Press Enter to stop all processes.

| Zone | Client port | Portals |
|------|-------------|---------|
| `starter_city` | 9100 | North (32,8) → Hunting Grounds |
| `starter_hunting` | 9101 | South (64,8) → City; East (120,64) → Goblin Cave |
| `starter_dungeon` | 9102 | West (4,24) → Hunting Grounds |

Optional: `-Zones starter_city` to run a single zone for isolated debugging.

All processes share `config/tbeq.db`. Closing the client saves your location and inventory; logging in again restores them.

## Run client

From the repo root (so `config/ui_layout.json` resolves correctly):

```powershell
.\build\ninja-debug-client\client\tbeq_client.exe
```

### Phase 0 client UI

- **HUD** and **Chat** shell windows are draggable and resizable
- Layout persists to `config/ui_layout.json` on exit
- **F1** toggles the debug menu (Debug builds, hidden by default) with a live spdlog log viewer
- **Phase 3.5** — procedural entity sprites on the tilemap (players, other clients, zone NPCs)
- **Phase 5** — `I` inventory, `L` look/inspect, `N` NPC interact, merchant and lorekeeper NPCs
- **Phase 5.5** — `P` portal on portal tiles; zone-aware portal hints in system chat
- **Phase 6** — `K` skills window; XP/level-up and skill-up notifications in system chat

## Project layout

| Path | Purpose |
|------|---------|
| `shared/` | Logging, config, binary packet serializers, domain stubs |
| `server/world_login/` | World/Login server — zone registry |
| `server/zone/` | Zone server — registers with WorldLogin |
| `client/` | SDL2 + ImGui demo client |
| `tools/data_validator/` | JSON content validation (CTest) |
| `tests/` | Catch2 unit + integration tests |
| `data/` | Seed JSON content |
| `scripts/run-dev.ps1` | Build + cluster + client in one command |
| `scripts/run_cluster.ps1` | Local dev cluster launcher (multi-zone) |

## Dev flags

| Flag | Process | Effect |
|------|---------|--------|
| `--dev-mode` | WorldLogin, ZoneServer | Enables server-side debug command handling |
| `--zone-id` | ZoneServer | Zone identifier for registration |
| `--port` / `--world-login-port` | WorldLogin / ZoneServer | WorldLogin listen port |
| `--client-port` | ZoneServer | Client gameplay listen port |

## CI

GitHub Actions workflow at `.github/workflows/ci.yml` bootstraps third-party archives, builds Release with tests, and runs `ctest --output-on-failure`.
