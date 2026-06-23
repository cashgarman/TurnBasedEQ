#include "tbeq/persistence/Database.hpp"

#include <chrono>
#include <sstream>

#include <sqlite3.h>
#include <spdlog/spdlog.h>

namespace tbeq::db
{

namespace
{

int64_t nowUnixSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string makeCharacterId(int64_t accountId, const std::string& name)
{
    std::ostringstream stream;
    stream << "c" << accountId << "_" << name;
    return stream.str();
}

bool bindText(sqlite3_stmt* stmt, int index, const std::string& value)
{
    return sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
}

} // namespace

Database::Database(std::string path)
    : path_(std::move(path))
{
}

Database::~Database()
{
    close();
}

bool Database::open()
{
    if (db_ != nullptr)
    {
        return true;
    }

    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK)
    {
        spdlog::error("Failed to open database {}: {}", path_, sqlite3_errmsg(db_));
        close();
        return false;
    }

    exec("PRAGMA foreign_keys = ON;");
    exec("PRAGMA journal_mode = WAL;");
    exec("PRAGMA busy_timeout = 3000;");
    return true;
}

void Database::close()
{
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::isOpen() const
{
    return db_ != nullptr;
}

bool Database::exec(const std::string& sql) const
{
    char* errorMessage = nullptr;
    const int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK)
    {
        spdlog::error("SQL error: {}", errorMessage != nullptr ? errorMessage : "unknown");
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}

namespace
{

bool columnExists(sqlite3* db, const char* table, const char* column)
{
    sqlite3_stmt* stmt = nullptr;
    const std::string sql = "PRAGMA table_info(" + std::string(table) + ");";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name != nullptr && std::string(name) == column)
        {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

bool execSql(sqlite3* db, const char* sql)
{
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK)
    {
        if (errMsg != nullptr)
        {
            sqlite3_free(errMsg);
        }
        return false;
    }
    return true;
}

bool migrateSchemaPhase56(sqlite3* db)
{
    if (!columnExists(db, "zones", "zone_type"))
    {
        if (!execSql(db, "ALTER TABLE zones ADD COLUMN zone_type TEXT NOT NULL DEFAULT 'city';"))
        {
            return false;
        }
    }

    static const char* kZoneLinksSql = R"SQL(
CREATE TABLE IF NOT EXISTS zone_links (
    link_id TEXT PRIMARY KEY,
    from_zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    to_zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    from_edge TEXT NOT NULL,
    to_edge TEXT NOT NULL,
    label TEXT
);
)SQL";

    if (!execSql(db, kZoneLinksSql))
    {
        return false;
    }

    return execSql(db, R"SQL(
UPDATE zones SET zone_type = CASE role
    WHEN 'starter_city' THEN 'city'
    WHEN 'hunting' THEN 'hunting'
    WHEN 'dungeon' THEN 'dungeon'
    ELSE zone_type
END;
)SQL");
}

} // namespace

bool Database::initializeSchema()
{
    static const char* kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    is_dev INTEGER NOT NULL DEFAULT 0,
    is_banned INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    token INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    expires_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS characters (
    id TEXT PRIMARY KEY,
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    race_id TEXT NOT NULL,
    class_id TEXT NOT NULL,
    level INTEGER NOT NULL DEFAULT 1,
    zone_id TEXT NOT NULL,
    pos_x REAL NOT NULL DEFAULT 32.0,
    pos_y REAL NOT NULL DEFAULT 32.0,
    state_json TEXT NOT NULL DEFAULT '{}',
    created_at INTEGER NOT NULL,
    UNIQUE(account_id, name)
);

CREATE TABLE IF NOT EXISTS worlds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    seed INTEGER NOT NULL,
    version TEXT NOT NULL,
    generated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS zones (
    id TEXT PRIMARY KEY,
    world_id INTEGER NOT NULL REFERENCES worlds(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    role TEXT NOT NULL,
    biome TEXT NOT NULL,
    tile_style TEXT NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    is_safe INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS zone_tiles (
    zone_id TEXT PRIMARY KEY REFERENCES zones(id) ON DELETE CASCADE,
    tile_data TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS zone_portals (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    tile_x INTEGER NOT NULL,
    tile_y INTEGER NOT NULL,
    dest_zone_id TEXT NOT NULL,
    dest_x INTEGER NOT NULL,
    dest_y INTEGER NOT NULL,
    label TEXT
);

CREATE TABLE IF NOT EXISTS zone_spawns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    spawn_id TEXT NOT NULL,
    tile_x INTEGER NOT NULL,
    tile_y INTEGER NOT NULL,
    mob_table TEXT NOT NULL,
    respawn_s INTEGER NOT NULL DEFAULT 60
);

CREATE TABLE IF NOT EXISTS zone_npc_slots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    slot_type TEXT NOT NULL,
    tile_x INTEGER NOT NULL,
    tile_y INTEGER NOT NULL,
    npc_id TEXT
);

CREATE TABLE IF NOT EXISTS zone_triggers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    zone_id TEXT NOT NULL REFERENCES zones(id) ON DELETE CASCADE,
    trigger_id TEXT NOT NULL,
    tile_x INTEGER NOT NULL,
    tile_y INTEGER NOT NULL,
    trigger_type TEXT NOT NULL,
    payload_json TEXT
);
)SQL";

    if (!exec(kSchemaSql))
    {
        return false;
    }

    return migrateSchemaPhase56(db_);
}

bool Database::clearWorld()
{
    return exec("DELETE FROM zone_triggers;")
        && exec("DELETE FROM zone_npc_slots;")
        && exec("DELETE FROM zone_spawns;")
        && exec("DELETE FROM zone_portals;")
        && exec("DELETE FROM zone_tiles;")
        && exec("DELETE FROM zone_links;")
        && exec("DELETE FROM zones;")
        && exec("DELETE FROM worlds;");
}

bool Database::hasWorld() const
{
    return worldCount() > 0;
}

int64_t Database::worldCount() const
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM worlds;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool Database::createAccount(const std::string& username, const std::string& passwordHash, const std::string& passwordSalt, bool isDev)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO accounts (username, password_hash, password_salt, is_dev, is_banned, created_at) VALUES (?, ?, ?, ?, 0, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, username);
    bindText(stmt, 2, passwordHash);
    bindText(stmt, 3, passwordSalt);
    sqlite3_bind_int(stmt, 4, isDev ? 1 : 0);
    sqlite3_bind_int64(stmt, 5, nowUnixSeconds());

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::optional<AccountRecord> Database::findAccountByUsername(const std::string& username) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, username, password_hash, password_salt, is_dev, is_banned FROM accounts WHERE username = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    bindText(stmt, 1, username);
    std::optional<AccountRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AccountRecord value;
        value.id = sqlite3_column_int64(stmt, 0);
        value.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        value.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        value.passwordSalt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        value.isDev = sqlite3_column_int(stmt, 4) != 0;
        value.isBanned = sqlite3_column_int(stmt, 5) != 0;
        record = std::move(value);
    }
    sqlite3_finalize(stmt);
    return record;
}

std::optional<AccountRecord> Database::findAccountById(int64_t accountId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, username, password_hash, password_salt, is_dev, is_banned FROM accounts WHERE id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    std::optional<AccountRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        AccountRecord value;
        value.id = sqlite3_column_int64(stmt, 0);
        value.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        value.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        value.passwordSalt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        value.isDev = sqlite3_column_int(stmt, 4) != 0;
        value.isBanned = sqlite3_column_int(stmt, 5) != 0;
        record = std::move(value);
    }
    sqlite3_finalize(stmt);
    return record;
}

bool Database::createSession(uint64_t token, int64_t accountId, int64_t expiresAt)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO sessions (token, account_id, expires_at) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(token));
    sqlite3_bind_int64(stmt, 2, accountId);
    sqlite3_bind_int64(stmt, 3, expiresAt);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::optional<SessionRecord> Database::findSession(uint64_t token) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT token, account_id, expires_at FROM sessions WHERE token = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(token));
    std::optional<SessionRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        SessionRecord value;
        value.token = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        value.accountId = sqlite3_column_int64(stmt, 1);
        value.expiresAt = sqlite3_column_int64(stmt, 2);
        record = std::move(value);
    }
    sqlite3_finalize(stmt);
    return record;
}

void Database::deleteSession(uint64_t token)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(token));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void Database::deleteExpiredSessions(int64_t now)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM sessions WHERE expires_at <= ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return;
    }
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::string Database::createCharacter(
    int64_t accountId,
    const std::string& name,
    const std::string& raceId,
    const std::string& classId,
    const std::string& startZone)
{
    const std::string characterId = makeCharacterId(accountId, name);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO characters (id, account_id, name, race_id, class_id, level, zone_id, pos_x, pos_y, state_json, created_at)
VALUES (?, ?, ?, ?, ?, 1, ?, 32.0, 32.0, '{}', ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return {};
    }

    bindText(stmt, 1, characterId);
    sqlite3_bind_int64(stmt, 2, accountId);
    bindText(stmt, 3, name);
    bindText(stmt, 4, raceId);
    bindText(stmt, 5, classId);
    bindText(stmt, 6, startZone);
    sqlite3_bind_int64(stmt, 7, nowUnixSeconds());

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? characterId : std::string{};
}

std::vector<CharacterSummary> Database::listCharacters(int64_t accountId) const
{
    std::vector<CharacterSummary> characters;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, name, race_id, class_id, level, zone_id FROM characters WHERE account_id = ? ORDER BY name;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return characters;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CharacterSummary summary;
        summary.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        summary.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        summary.raceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        summary.classId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        summary.level = static_cast<uint16_t>(sqlite3_column_int(stmt, 4));
        summary.zoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        characters.push_back(std::move(summary));
    }
    sqlite3_finalize(stmt);
    return characters;
}

std::optional<CharacterRecord> Database::findCharacter(const std::string& characterId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT id, account_id, name, race_id, class_id, level, zone_id, pos_x, pos_y, state_json
FROM characters WHERE id = ? LIMIT 1;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    bindText(stmt, 1, characterId);
    std::optional<CharacterRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        CharacterRecord value;
        value.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        value.accountId = sqlite3_column_int64(stmt, 1);
        value.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        value.raceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        value.classId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        value.level = static_cast<uint16_t>(sqlite3_column_int(stmt, 5));
        value.zoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        value.posX = static_cast<float>(sqlite3_column_double(stmt, 7));
        value.posY = static_cast<float>(sqlite3_column_double(stmt, 8));
        value.stateJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        record = std::move(value);
    }
    sqlite3_finalize(stmt);
    return record;
}

std::optional<CharacterRecord> Database::findCharacterForAccount(int64_t accountId, const std::string& characterId) const
{
    auto record = findCharacter(characterId);
    if (!record.has_value() || record->accountId != accountId)
    {
        return std::nullopt;
    }
    return record;
}

int64_t Database::characterCountForAccount(int64_t accountId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM characters WHERE account_id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool Database::insertWorld(int64_t seed, const std::string& version, int64_t generatedAt, int64_t& outWorldId)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO worlds (seed, version, generated_at) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, seed);
    bindText(stmt, 2, version);
    sqlite3_bind_int64(stmt, 3, generatedAt);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    outWorldId = sqlite3_last_insert_rowid(db_);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::insertZone(
    const std::string& zoneId,
    int64_t worldId,
    const std::string& name,
    const std::string& role,
    const std::string& zoneType,
    const std::string& biome,
    const std::string& tileStyle,
    int32_t width,
    int32_t height,
    bool isSafe)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO zones (id, world_id, name, role, zone_type, biome, tile_style, width, height, is_safe)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    sqlite3_bind_int64(stmt, 2, worldId);
    bindText(stmt, 3, name);
    bindText(stmt, 4, role);
    bindText(stmt, 5, zoneType);
    bindText(stmt, 6, biome);
    bindText(stmt, 7, tileStyle);
    sqlite3_bind_int(stmt, 8, width);
    sqlite3_bind_int(stmt, 9, height);
    sqlite3_bind_int(stmt, 10, isSafe ? 1 : 0);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::insertZoneLink(const ZoneLinkRecord& link)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO zone_links (link_id, from_zone_id, to_zone_id, from_edge, to_edge, label)
VALUES (?, ?, ?, ?, ?, ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, link.linkId);
    bindText(stmt, 2, link.fromZoneId);
    bindText(stmt, 3, link.toZoneId);
    bindText(stmt, 4, link.fromEdge);
    bindText(stmt, 5, link.toEdge);
    bindText(stmt, 6, link.label);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::hasZoneContent(const std::string& zoneId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT 1 FROM zone_tiles WHERE zone_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

bool Database::beginTransaction() const
{
    return exec("BEGIN IMMEDIATE;");
}

bool Database::commitTransaction() const
{
    return exec("COMMIT;");
}

bool Database::rollbackTransaction() const
{
    return exec("ROLLBACK;");
}

bool Database::insertZoneTiles(const std::string& zoneId, const std::string& tileDataJson)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO zone_tiles (zone_id, tile_data) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    bindText(stmt, 2, tileDataJson);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::insertZonePortal(const ZonePortalRecord& portal)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO zone_portals (zone_id, tile_x, tile_y, dest_zone_id, dest_x, dest_y, label)
VALUES (?, ?, ?, ?, ?, ?, ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, portal.zoneId);
    sqlite3_bind_int(stmt, 2, portal.tileX);
    sqlite3_bind_int(stmt, 3, portal.tileY);
    bindText(stmt, 4, portal.destZoneId);
    sqlite3_bind_int(stmt, 5, portal.destX);
    sqlite3_bind_int(stmt, 6, portal.destY);
    bindText(stmt, 7, portal.label);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::insertZoneSpawn(
    const std::string& zoneId,
    const std::string& spawnId,
    int32_t tileX,
    int32_t tileY,
    const std::string& mobTable,
    int32_t respawnSeconds)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO zone_spawns (zone_id, spawn_id, tile_x, tile_y, mob_table, respawn_s)
VALUES (?, ?, ?, ?, ?, ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    bindText(stmt, 2, spawnId);
    sqlite3_bind_int(stmt, 3, tileX);
    sqlite3_bind_int(stmt, 4, tileY);
    bindText(stmt, 5, mobTable);
    sqlite3_bind_int(stmt, 6, respawnSeconds);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::insertZoneNpcSlot(
    const std::string& zoneId,
    const std::string& slotType,
    int32_t tileX,
    int32_t tileY,
    const std::string& npcId)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
INSERT INTO zone_npc_slots (zone_id, slot_type, tile_x, tile_y, npc_id)
VALUES (?, ?, ?, ?, ?);
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    bindText(stmt, 2, slotType);
    sqlite3_bind_int(stmt, 3, tileX);
    sqlite3_bind_int(stmt, 4, tileY);
    bindText(stmt, 5, npcId);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateNpcSlot(int64_t slotId, const std::string& npcId)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE zone_npc_slots SET npc_id = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, npcId);
    sqlite3_bind_int64(stmt, 2, slotId);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<std::string> Database::listZoneIds() const
{
    std::vector<std::string> zoneIds;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id FROM zones ORDER BY id;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return zoneIds;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        zoneIds.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return zoneIds;
}

std::optional<std::string> Database::loadZoneTiles(const std::string& zoneId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT tile_data FROM zone_tiles WHERE zone_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    bindText(stmt, 1, zoneId);
    std::optional<std::string> tileData;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        tileData = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return tileData;
}

std::optional<ZoneMetadataRecord> Database::loadZoneMetadata(const std::string& zoneId) const
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT z.id, z.name, z.zone_type, z.role, z.biome, z.tile_style, z.width, z.height, z.is_safe,
       z.world_id, w.seed
FROM zones z
JOIN worlds w ON w.id = z.world_id
WHERE z.id = ? LIMIT 1;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return std::nullopt;
    }

    bindText(stmt, 1, zoneId);
    std::optional<ZoneMetadataRecord> metadata;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ZoneMetadataRecord record;
        record.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.zoneType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record.biome = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        record.tileStyle = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        record.width = sqlite3_column_int(stmt, 6);
        record.height = sqlite3_column_int(stmt, 7);
        record.isSafe = sqlite3_column_int(stmt, 8) != 0;
        record.worldId = sqlite3_column_int64(stmt, 9);
        record.worldSeed = sqlite3_column_int64(stmt, 10);
        metadata = std::move(record);
    }
    sqlite3_finalize(stmt);
    return metadata;
}

std::vector<ZoneLinkRecord> Database::loadAllZoneLinks() const
{
    std::vector<ZoneLinkRecord> links;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT link_id, from_zone_id, to_zone_id, from_edge, to_edge, COALESCE(label, '')
FROM zone_links ORDER BY link_id;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return links;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ZoneLinkRecord link;
        link.linkId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        link.fromZoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        link.toZoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        link.fromEdge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        link.toEdge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        link.label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        links.push_back(std::move(link));
    }
    sqlite3_finalize(stmt);
    return links;
}

std::vector<ZoneLinkRecord> Database::loadOutgoingZoneLinks(const std::string& zoneId) const
{
    std::vector<ZoneLinkRecord> links;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT link_id, from_zone_id, to_zone_id, from_edge, to_edge, COALESCE(label, '')
FROM zone_links WHERE from_zone_id = ? ORDER BY link_id;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return links;
    }

    bindText(stmt, 1, zoneId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ZoneLinkRecord link;
        link.linkId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        link.fromZoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        link.toZoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        link.fromEdge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        link.toEdge = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        link.label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        links.push_back(std::move(link));
    }
    sqlite3_finalize(stmt);
    return links;
}

std::vector<NpcSlotRecord> Database::loadZoneNpcSlots(const std::string& zoneId) const
{
    std::vector<NpcSlotRecord> slots;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT slot_type, tile_x, tile_y, COALESCE(npc_id, '')
FROM zone_npc_slots WHERE zone_id = ?;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return slots;
    }

    bindText(stmt, 1, zoneId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        NpcSlotRecord slot;
        slot.slotType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        slot.tileX = sqlite3_column_int(stmt, 1);
        slot.tileY = sqlite3_column_int(stmt, 2);
        slot.npcId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        slots.push_back(std::move(slot));
    }
    sqlite3_finalize(stmt);
    return slots;
}

std::vector<ZoneSpawnRecord> Database::loadZoneSpawns(const std::string& zoneId) const
{
    std::vector<ZoneSpawnRecord> spawns;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT spawn_id, tile_x, tile_y, mob_table, respawn_s
FROM zone_spawns WHERE zone_id = ?;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return spawns;
    }

    bindText(stmt, 1, zoneId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ZoneSpawnRecord spawn;
        spawn.spawnId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        spawn.tileX = sqlite3_column_int(stmt, 1);
        spawn.tileY = sqlite3_column_int(stmt, 2);
        spawn.mobTable = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        spawn.respawnSeconds = sqlite3_column_int(stmt, 4);
        spawns.push_back(std::move(spawn));
    }
    sqlite3_finalize(stmt);
    return spawns;
}

bool Database::updateCharacterLocation(const std::string& characterId, const std::string& zoneId, float posX, float posY)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE characters SET zone_id = ?, pos_x = ?, pos_y = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, zoneId);
    sqlite3_bind_double(stmt, 2, posX);
    sqlite3_bind_double(stmt, 3, posY);
    bindText(stmt, 4, characterId);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateCharacterState(const std::string& characterId, const std::string& stateJson)
{
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE characters SET state_json = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    bindText(stmt, 1, stateJson);
    bindText(stmt, 2, characterId);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<ZonePortalRecord> Database::loadZonePortals(const std::string& zoneId) const
{
    std::vector<ZonePortalRecord> portals;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"SQL(
SELECT zone_id, tile_x, tile_y, dest_zone_id, dest_x, dest_y, COALESCE(label, '')
FROM zone_portals WHERE zone_id = ?;
)SQL";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return portals;
    }

    bindText(stmt, 1, zoneId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ZonePortalRecord portal;
        portal.zoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        portal.tileX = sqlite3_column_int(stmt, 1);
        portal.tileY = sqlite3_column_int(stmt, 2);
        portal.destZoneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        portal.destX = sqlite3_column_int(stmt, 4);
        portal.destY = sqlite3_column_int(stmt, 5);
        portal.label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        portals.push_back(std::move(portal));
    }
    sqlite3_finalize(stmt);
    return portals;
}

} // namespace tbeq::db
