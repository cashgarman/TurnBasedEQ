#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "tbeq/content/SkillCatalog.hpp"
#include "tbeq/skills/SkillDef.hpp"
#include "tbeq/net/ClientPackets.hpp"

namespace tbeq::ui
{
class GameWindow;
}

namespace tbeq::client
{

class SkillsWindow
{
public:
    void setSkillCatalog(const content::SkillCatalog* catalog);
    void applySnapshot(const net::SkillsSnapshotPayload& snapshot);
    void applySkillGain(const net::SkillGainPayload& gain);
    void applyLevelUp(const net::LevelUpPayload& levelUp);
    void draw(
        tbeq::ui::GameWindow& window,
        bool& visible,
        int displayWidth,
        int displayHeight,
        const std::function<void(const std::string& line)>& appendLogLine);

private:
    std::string categoryLabel(tbeq::SkillCategory category) const;
    uint16_t skillCap(const std::string& skillId) const;

    const content::SkillCatalog* skillCatalog_ = nullptr;
    net::SkillsSnapshotPayload snapshot_;
    std::unordered_map<std::string, net::SkillEntryPayload> skillEntries_;
    std::vector<std::string> recentSkillUps_;
};

} // namespace tbeq::client
