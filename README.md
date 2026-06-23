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
.\build\client\Debug\tbeq_client.exe
```

### Phase 0 client UI

- **HUD** and **Chat** shell windows are draggable and resizable
- Layout persists to `config/ui_layout.json` on exit
- **F1** toggles the debug menu (Debug builds, hidden by default) with a live spdlog log viewer
- **Phase 3.5** — procedural entity sprites on the tilemap (players, other clients, zone NPCs)
- **Phase 5** — `I` inventory, `L` look/inspect, `N` NPC interact, merchant and lorekeeper NPCs
- **Phase 5.5** — `P` portal on portal tiles; zone-aware portal hints in system chat

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
