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

## Run local server cluster

```powershell
.\scripts\run_cluster.ps1 -BuildConfig Debug
```

Starts `tbeq_world_login` and `tbeq_zone_server` with `--dev-mode`. Press Enter to stop both processes.

## Run client

From the repo root (so `config/ui_layout.json` resolves correctly):

```powershell
.\build\client\Debug\tbeq_client.exe
```

### Phase 0 client UI

- **HUD** and **Chat** shell windows are draggable and resizable
- Layout persists to `config/ui_layout.json` on exit
- **F1** toggles the debug menu (Debug builds) with a live spdlog log viewer
- **Phase 3.5** — procedural entity sprites on the tilemap (players, other clients, zone NPCs)

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
| `scripts/run_cluster.ps1` | Local dev cluster launcher |

## Dev flags

| Flag | Process | Effect |
|------|---------|--------|
| `--dev-mode` | WorldLogin, ZoneServer | Enables server-side debug command handling |
| `--zone-id` | ZoneServer | Zone identifier for registration |
| `--port` / `--world-login-port` | WorldLogin / ZoneServer | WorldLogin listen port |
| `--client-port` | ZoneServer | Client gameplay listen port |

## CI

GitHub Actions workflow at `.github/workflows/ci.yml` bootstraps third-party archives, builds Release with tests, and runs `ctest --output-on-failure`.
