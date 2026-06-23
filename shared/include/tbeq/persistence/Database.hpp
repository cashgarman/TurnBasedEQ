#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace tbeq::db
{

struct AccountRecord
{
    int64_t id = 0;
    std::string username;
    std::string passwordHash;
    std::string passwordSalt;
    bool isDev = false;
    bool isBanned = false;
};

struct SessionRecord
{
    uint64_t token = 0;
    int64_t accountId = 0;
    int64_t expiresAt = 0;
};

struct CharacterRecord
{
    std::string id;
    int64_t accountId = 0;
    std::string name;
    std::string raceId;
    std::string classId;
    uint16_t level = 1;
    std::string zoneId;
    float posX = 32.0f;
    float posY = 32.0f;
    std::string stateJson;
};

struct CharacterSummary
{
    std::string id;
    std::string name;
    std::string raceId;
    std::string classId;
    uint16_t level = 1;
    std::string zoneId;
};

struct ZonePortalRecord
{
    std::string zoneId;
    int32_t tileX = 0;
    int32_t tileY = 0;
    std::string destZoneId;
    int32_t destX = 0;
    int32_t destY = 0;
    std::string label;
};

struct ZoneMetadataRecord
{
    std::string id;
    std::string name;
    std::string tileStyle;
    int32_t width = 0;
    int32_t height = 0;
    bool isSafe = false;
};

struct NpcSlotRecord
{
    std::string slotType;
    int32_t tileX = 0;
    int32_t tileY = 0;
    std::string npcId;
};

struct ZoneSpawnRecord
{
    std::string spawnId;
    int32_t tileX = 0;
    int32_t tileY = 0;
    std::string mobTable;
    int32_t respawnSeconds = 60;
};

class Database
{
public:
    explicit Database(std::string path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open();
    void close();
    bool isOpen() const;
    const std::string& path() const { return path_; }

    bool initializeSchema();
    bool clearWorld();
    bool hasWorld() const;
    int64_t worldCount() const;

    bool createAccount(const std::string& username, const std::string& passwordHash, const std::string& passwordSalt, bool isDev);
    std::optional<AccountRecord> findAccountByUsername(const std::string& username) const;
    std::optional<AccountRecord> findAccountById(int64_t accountId) const;

    bool createSession(uint64_t token, int64_t accountId, int64_t expiresAt);
    std::optional<SessionRecord> findSession(uint64_t token) const;
    void deleteSession(uint64_t token);
    void deleteExpiredSessions(int64_t now);

    std::string createCharacter(
        int64_t accountId,
        const std::string& name,
        const std::string& raceId,
        const std::string& classId,
        const std::string& startZone);
    std::vector<CharacterSummary> listCharacters(int64_t accountId) const;
    std::optional<CharacterRecord> findCharacter(const std::string& characterId) const;
    std::optional<CharacterRecord> findCharacterForAccount(int64_t accountId, const std::string& characterId) const;
    int64_t characterCountForAccount(int64_t accountId) const;

    bool insertWorld(int64_t seed, const std::string& version, int64_t generatedAt, int64_t& outWorldId);
    bool insertZone(
        const std::string& zoneId,
        int64_t worldId,
        const std::string& name,
        const std::string& role,
        const std::string& biome,
        const std::string& tileStyle,
        int32_t width,
        int32_t height,
        bool isSafe);
    bool insertZoneTiles(const std::string& zoneId, const std::string& tileDataJson);
    bool insertZonePortal(const ZonePortalRecord& portal);
    bool insertZoneSpawn(
        const std::string& zoneId,
        const std::string& spawnId,
        int32_t tileX,
        int32_t tileY,
        const std::string& mobTable,
        int32_t respawnSeconds);
    bool insertZoneNpcSlot(
        const std::string& zoneId,
        const std::string& slotType,
        int32_t tileX,
        int32_t tileY,
        const std::string& npcId);
    bool updateNpcSlot(int64_t slotId, const std::string& npcId);
    std::vector<std::string> listZoneIds() const;
    std::optional<std::string> loadZoneTiles(const std::string& zoneId) const;
    std::optional<ZoneMetadataRecord> loadZoneMetadata(const std::string& zoneId) const;
    std::vector<ZonePortalRecord> loadZonePortals(const std::string& zoneId) const;
    std::vector<NpcSlotRecord> loadZoneNpcSlots(const std::string& zoneId) const;
    std::vector<ZoneSpawnRecord> loadZoneSpawns(const std::string& zoneId) const;
    bool updateCharacterLocation(const std::string& characterId, const std::string& zoneId, float posX, float posY);
    bool updateCharacterState(const std::string& characterId, const std::string& stateJson);

private:
    bool exec(const std::string& sql) const;

    std::string path_;
    sqlite3* db_ = nullptr;
};

} // namespace tbeq::db
