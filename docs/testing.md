# Testing

TurnBasedEQ uses **Catch2 v3** for unit and integration tests, plus CTest targets for content validation. Tests link against `tbeq_shared` and optionally spawn real server executables.

See also: [build-and-run.md](build-and-run.md), [cpp-architecture.md](cpp-architecture.md).

---

## Test targets

Defined in `tests/CMakeLists.txt`:

| Target | Type | Main file |
|--------|------|-----------|
| `tbeq_tests_unit` | Catch2 unit | Multiple `tests/unit/**/*.cpp` |
| `tbeq_tests_integration` | Catch2 integration | `tests/integration/*.cpp` |
| `tbeq_test_helpers` | Static library | TestCluster, HeadlessClient, TempDatabase |

Additional CTest targets (via `tools/CMakeLists.txt`):

| CTest name | Command |
|------------|---------|
| `content_validation` | `tbeq_data_validator <repo>/data` |
| `worldgen_validation` | `tbeq_worldgen --seed 42 --validate-only` |

---

## Running tests

```powershell
# All tests
ctest --test-dir build -C Debug --output-on-failure

# Unit only
.\build\tests\Debug\tbeq_tests_unit.exe

# Integration only (requires built server exes)
.\build\tests\Debug\tbeq_tests_integration.exe
```

Integration tests use `RESOURCE_LOCK integration_servers` to prevent parallel port conflicts.

---

## Test helpers

### TestCluster

**Files:** `tests/helpers/TestCluster.hpp`, `TestCluster.cpp`

Spawns real server processes on ephemeral ports:

| Method | Purpose |
|--------|---------|
| `pickEphemeralPort()` | Find free TCP port |
| `spawnProcess(args, cwd)` | Launch exe, return `ProcessHandle` |
| `waitForTcpPort(port, timeout)` | Poll until listening |
| `startZoneServer(zoneId, port, outProcess)` | Launch zone server |
| `startDefaultZoneCluster(out)` | City + hunting + dungeon |

Uses temporary or shared SQLite via `TempDatabase`.

### ProcessHandle

Move-only RAII wrapper — terminates process on destruction. Prevents orphaned server processes after test failures.

### HeadlessClient

**Files:** `tests/helpers/HeadlessClient.hpp`, `HeadlessClient.cpp`

ASIO TCP client implementing the login and gameplay protocol without SDL. Used for integration tests that need full stack verification.

### TempDatabase

Creates isolated SQLite file for tests that mutate accounts/characters.

---

## Unit test coverage

| Directory | Tags | Topics |
|-----------|------|--------|
| `unit/net/` | `[net]` | Packet round-trip serialization |
| `unit/auth/` | | Password hash, validation |
| `unit/shared/` | `[combat]` | Combat formulas, equip rules, merchant pricing |
| `unit/ai/` | | ClassCombatBrain decisions |
| `unit/world/` | | ZoneGrid collision |
| `unit/worldgen/` | | World validator |
| `unit/render/` | `[render]` | Tile/sprite generators |
| `unit/server/` | | Debug command handler |
| `unit/ui/` | | Window layout persistence |

Example combat test pattern (seeded RNG):

```cpp
std::mt19937 rng(12345);
combat::CombatInstance instance(id, skillResolver, mobs, spells, abilities, rng);
```

---

## Integration test coverage

| File | Scenario |
|------|----------|
| `cluster_registers_test.cpp` | Zone registers with WorldLogin |
| `login_and_handoff_test.cpp` | Account login → character select → zone entry |
| `two_players_and_portal_test.cpp` | Multi-player + portal travel |
| `combat_encounter_test.cpp` | Debug spawn combat, assert packets |
| `ai_party_combat_test.cpp` | AI companion participates in combat |
| `merchant_buy_test.cpp` | Buy item from merchant NPC |
| `lorekeeper_interact_test.cpp` | Lorekeeper dialog via interact |

Typical flow:

1. `TestCluster` starts WorldLogin + zone(s)
2. `HeadlessClient` creates account, logs in, selects character
3. Connects to zone, sends debug command or gameplay action
4. Asserts on received packet payloads

---

## Client code in unit tests

Some unit tests compile client sources directly:

- `GameWindow.cpp`, `WindowLayoutManager.cpp` — UI layout tests
- `TileGenerator.cpp`, `SpriteGenerator.cpp` — render tests

This validates client logic without launching the full SDL application.

---

## Debug menu unit test runner

`client/ui/debug/UnitTestRunner.cpp` can invoke Catch2 tests from the in-client debug menu (Debug builds). Uses `TBEQ_REPO_ROOT` compile definition to locate test binary.

---

## CI integration

GitHub Actions runs Release build with `TBEQ_BUILD_TESTS=ON` and `ctest --output-on-failure`. See `.github/workflows/ci.yml`.

---

## Writing new tests

### Unit test template

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("description", "[tag]")
{
    // Arrange shared domain objects
    // Act
    // Assert with REQUIRE / CHECK
}
```

### Integration test requirements

1. Build server executables first
2. Use `TestCluster` for process lifecycle
3. Use ephemeral ports — never hardcode 9000/9100
4. Tag appropriately; integration exe uses resource lock

---

## Related documentation

- [networking.md](networking.md) — protocol tested by round-trip tests
- [combat-system.md](combat-system.md) — combat unit tests
- [build-and-run.md](build-and-run.md) — ctest commands
- [server.md](server.md) — processes spawned by TestCluster
