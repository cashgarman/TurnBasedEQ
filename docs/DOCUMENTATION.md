# TurnBasedEQ Documentation Index

Welcome to the TurnBasedEQ documentation set. This index links all project docs and describes what each covers.

TurnBasedEQ is a greenfield C++20 online MMO foundation: a distributed server cluster (WorldLogin + Zone servers), a shared domain library, binary TCP protocol, SQLite persistence, an SDL2 + ImGui client, and Catch2 tests.

---

## Quick start

| Goal | Document |
|------|----------|
| Build, run cluster, and launch client | [build-and-run.md](build-and-run.md) |
| Understand system shape at a glance | [architecture-overview.md](architecture-overview.md) |
| Onboard as a C++ contributor | [cpp-architecture.md](cpp-architecture.md) (existing) |

---

## Documentation map

### Getting started

| Document | Description |
|----------|-------------|
| [build-and-run.md](build-and-run.md) | Prerequisites, CMake build, dev cluster, client launch, CLI flags, CI |
| [architecture-overview.md](architecture-overview.md) | High-level topology, process roles, data flow, zone cluster layout |

### Core systems

| Document | Description |
|----------|-------------|
| [server.md](server.md) | WorldLoginServer, ZoneServer, TCP layer, debug commands, zone transfer |
| [client.md](client.md) | SDL2/ImGui client, rendering, UI windows, input bindings |
| [shared.md](shared.md) | `tbeq_shared` library: namespaces, catalogs, combat, skills, persistence |
| [networking.md](networking.md) | Packet framing, protocol types, login/handoff flows, serialization |
| [auth.md](auth.md) | Accounts, sessions, password hashing, character validation |
| [combat-system.md](combat-system.md) | Turn-based combat, initiative, actions, AI, loot, server integration |
| [data-models.md](data-models.md) | Character state, DB schema, entities, zone grid, persistence patterns |
| [content-and-data.md](content-and-data.md) | JSON catalogs, world generation, data validator, seed content |

### Reference

| Document | Description |
|----------|-------------|
| [module-reference.md](module-reference.md) | Module-by-module file and class reference |
| [testing.md](testing.md) | Unit/integration tests, TestCluster, HeadlessClient, CTest targets |
| [cpp-architecture.md](cpp-architecture.md) | C++20 patterns, ownership, concurrency, design tradeoffs (existing) |

### External

| Resource | Description |
|----------|-------------|
| [../README.md](../README.md) | Project README with phase feature list and quick commands |

---

## Repository layout (summary)

```
TurnBasedEQ/
├── client/           SDL2 + ImGui game client
├── server/
│   ├── world_login/  Login, auth, zone registry, handoff broker
│   ├── zone/         Gameplay simulation per zone
│   └── common/       Shared server TCP and debug utilities
├── shared/           tbeq_shared — domain logic used by client, server, tests
├── data/             JSON seed content and worldgen rules
├── config/           Runtime config (SQLite DB, UI layout)
├── tools/            data_validator, worldgen CLI
├── tests/            Catch2 unit and integration tests
├── scripts/          bootstrap, run_cluster.ps1
└── docs/             This documentation set
```

---

## Suggested reading order

1. [architecture-overview.md](architecture-overview.md) — how pieces connect
2. [build-and-run.md](build-and-run.md) — get a local cluster running
3. [networking.md](networking.md) — protocol and login flow
4. [server.md](server.md) + [client.md](client.md) — runtime behavior
5. [combat-system.md](combat-system.md) + [data-models.md](data-models.md) — game domain
6. [module-reference.md](module-reference.md) — drill into specific files
7. [cpp-architecture.md](cpp-architecture.md) — engineering conventions

---

## Documentation conventions

- **Paths** are relative to the repository root unless noted.
- **Executables** (Windows): `tbeq_world_login.exe`, `tbeq_zone_server.exe`, `tbeq_client.exe`.
- **Namespaces** follow directory boundaries: `tbeq::net`, `tbeq::combat`, `tbeq::server`, etc.
- Cross-links use relative markdown paths within `docs/`.
