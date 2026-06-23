# Build and Run

This guide covers prerequisites, building TurnBasedEQ, running the local dev cluster, launching the client, and running tests.

---

## Prerequisites (Windows)

| Requirement | Notes |
|-------------|-------|
| Visual Studio 2022 | C++ desktop development workload |
| CMake 3.24+ | Generator for VS 2022 |
| vcpkg | Manifest mode; provides **imgui** |
| PowerShell | Used by bootstrap and cluster scripts |

Most other dependencies are vendored from archives in `third_party/cache/` (ASIO, SDL2, spdlog, Catch2, sqlite, nlohmann-json, fmt). ImGui SDL2 backend sources live in `third_party/imgui-src/backends/`.

---

## Bootstrap third-party dependencies

From the repository root:

```powershell
cd i:\TurnBasedEQ
.\scripts\bootstrap_third_party.ps1
```

This extracts cached source archives required by CMake `FetchContent`.

---

## Configure and build

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Debug `
  -DTBEQ_BUILD_TESTS=ON

cmake --build build --config Debug --parallel
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `TBEQ_BUILD_TESTS` | ON | Build Catch2 unit/integration tests and tools |
| `TBEQ_ENABLE_DEBUG_MENU` | ON in Debug, OFF in Release | Client F1 debug menu with log viewer and cheats |

Release builds disable the client debug menu unless explicitly enabled.

### Build outputs

| Target | Output path (Debug) |
|--------|---------------------|
| `tbeq_world_login` | `build/server/Debug/tbeq_world_login.exe` |
| `tbeq_zone_server` | `build/server/Debug/tbeq_zone_server.exe` |
| `tbeq_client` | `build/client/Debug/tbeq_client.exe` |
| `tbeq_tests_unit` | `build/tests/Debug/tbeq_tests_unit.exe` |
| `tbeq_tests_integration` | `build/tests/Debug/tbeq_tests_integration.exe` |
| `tbeq_data_validator` | `build/tools/Debug/tbeq_data_validator.exe` |
| `tbeq_worldgen` | `build/tools/Debug/tbeq_worldgen.exe` |

---

## Run tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

CTest includes:

- Catch2 unit tests (`tbeq_tests_unit`)
- Catch2 integration tests (`tbeq_tests_integration`) — uses `RESOURCE_LOCK integration_servers`
- `content_validation` — JSON schema checks via `tbeq_data_validator`
- `worldgen_validation` — worldgen dry-run with seed 42

---

## Run local server cluster

```powershell
.\scripts\run_cluster.ps1 -BuildConfig Debug
```

This script:

1. Stops stale `tbeq_world_login` / `tbeq_zone_server` processes on configured ports
2. Starts WorldLogin with `--dev-mode`
3. Starts three zone servers (city, hunting, dungeon) in separate console windows
4. Waits for TCP ports to become ready
5. Blocks until Enter is pressed, then stops all processes

### Script parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `-BuildConfig` | `Debug` | CMake configuration folder |
| `-Zones` | all three starter zones | Restrict to one zone for isolated debugging |
| `-WorldLoginPort` | 9000 | Zone registration port |
| `-WorldLoginClientPort` | 9001 | Client login port |
| `-ZoneClientPortBase` | 9100 | First zone client port (incremented per zone) |
| `-DbPath` | `config/tbeq.db` | Shared SQLite database |

Example — single zone only:

```powershell
.\scripts\run_cluster.ps1 -BuildConfig Debug -Zones starter_city
```

---

## Run client

Run from the **repository root** so relative paths resolve correctly (`config/ui_layout.json`, `data/`):

```powershell
.\build\client\Debug\tbeq_client.exe
```

The client can optionally spawn a local cluster via `LocalClusterLauncher` during development (see [client.md](client.md)).

### Client key bindings (summary)

| Key | Action |
|-----|--------|
| F1 | Toggle debug menu (Debug builds only) |
| I | Toggle inventory window |
| L | Toggle look/inspect panel |
| N | Interact with nearby NPC |
| P | Use portal on current tile |
| WASD / arrows | Move (when in zone) |

---

## Server CLI flags

Parsed by `tbeq::parseServerArgs()` in `shared/src/core/Config.cpp`:

| Flag | Process | Effect |
|------|---------|--------|
| `--dev-mode` | WorldLogin, ZoneServer | Enables debug command handling |
| `--zone-id <id>` | ZoneServer | Zone identifier for registration and logging |
| `--port <n>` | WorldLogin | WorldLogin zone listener port |
| `--world-login-port <n>` | ZoneServer | Port to connect for registration |
| `--client-port <n>` | ZoneServer | Client gameplay listen port |
| `--db-path <path>` | All servers | SQLite database path |
| `--data-root <path>` | All servers | JSON content root (default `data`) |

Example manual zone server start:

```powershell
.\build\server\Debug\tbeq_zone_server.exe `
  --zone-id starter_city `
  --world-login-port 9000 `
  --client-port 9100 `
  --dev-mode
```

---

## Logging

| Process | Log file |
|---------|----------|
| WorldLogin | `logs/world_login.log` |
| Zone server | `logs/zone_<zone_id>.log` (e.g. `zone_starter_city.log`) |
| Client | `logs/client.log` |

Zone servers set the console title to `TBEQ Zone: <zone_id>` on Windows.

---

## Database and first-run world generation

On first startup, WorldLogin calls `worldgen::ensureWorldInDatabase()` which generates zone tiles, portals, spawns, and NPC slots into `config/tbeq.db` using the configured world seed (default `42`).

To reset world data, delete `config/tbeq.db` and restart the cluster.

---

## CI pipeline

GitHub Actions (`.github/workflows/ci.yml`) on `windows-latest`:

1. Bootstrap third-party cache
2. Clone and bootstrap vcpkg
3. Configure Release with tests
4. Build and run `ctest --output-on-failure`

---

## Troubleshooting

| Issue | Check |
|-------|-------|
| Port already in use | `run_cluster.ps1` warns and skips non-TBEQ processes; stop conflicting apps or change ports |
| Missing imgui | Ensure vcpkg toolchain is passed to CMake |
| Client can't find layout | Run client from repo root |
| Zone registration fails | WorldLogin must be running before zone servers start |
| Empty world | Delete DB and restart; verify `data/worldgen/` exists |

---

## Related documentation

- [architecture-overview.md](architecture-overview.md) — process topology
- [server.md](server.md) — server behavior
- [client.md](client.md) — client startup flow
- [testing.md](testing.md) — test suite details
