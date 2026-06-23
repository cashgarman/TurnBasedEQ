#include "skills/SkillsWindow.hpp"

#include <algorithm>

#include <imgui.h>

#include "ui/GameWindow.hpp"
#include "tbeq/content/SkillCatalog.hpp"

namespace tbeq::client
{

void SkillsWindow::setSkillCatalog(const content::SkillCatalog* catalog)
{
    skillCatalog_ = catalog;
}

void SkillsWindow::applySnapshot(const net::SkillsSnapshotPayload& snapshot)
{
    snapshot_ = snapshot;
    skillEntries_.clear();
    for (const auto& entry : snapshot.skills)
    {
        skillEntries_[entry.skillId] = entry;
    }
}

void SkillsWindow::applySkillGain(const net::SkillGainPayload& gain)
{
    auto it = skillEntries_.find(gain.skillId);
    if (it != skillEntries_.end())
    {
        it->second.level = gain.newLevel;
    }
    recentSkillUps_.push_back(gain.skillId);
    if (recentSkillUps_.size() > 8)
    {
        recentSkillUps_.erase(recentSkillUps_.begin());
    }
}

void SkillsWindow::applyLevelUp(const net::LevelUpPayload& levelUp)
{
    snapshot_.characterLevel = levelUp.newLevel;
    for (auto& [skillId, entry] : skillEntries_)
    {
        entry.cap = skillCap(skillId);
        (void)skillId;
    }
}

std::string SkillsWindow::categoryLabel(tbeq::SkillCategory category) const
{
    switch (category)
    {
    case tbeq::SkillCategory::MeleeWeapon:
        return "Melee Weapons";
    case tbeq::SkillCategory::CombatFundamentals:
        return "Combat Fundamentals";
    case tbeq::SkillCategory::CombatManeuvers:
        return "Combat Maneuvers";
    case tbeq::SkillCategory::Casting:
        return "Casting";
    case tbeq::SkillCategory::CastingSupport:
        return "Casting Support";
    case tbeq::SkillCategory::StealthUtility:
        return "Stealth & Utility";
    case tbeq::SkillCategory::Exploration:
        return "Exploration";
    case tbeq::SkillCategory::Support:
        return "Support";
    case tbeq::SkillCategory::Crafting:
        return "Crafting";
    case tbeq::SkillCategory::Trade:
        return "Trade";
    default:
        return "Other";
    }
}

uint16_t SkillsWindow::skillCap(const std::string& skillId) const
{
    const auto it = skillEntries_.find(skillId);
    if (it == skillEntries_.end())
    {
        return 0;
    }
    return it->second.cap;
}

void SkillsWindow::draw(
    tbeq::ui::GameWindow& window,
    bool& visible,
    int displayWidth,
    int displayHeight,
    const std::function<void(const std::string& line)>& appendLogLine)
{
    (void)appendLogLine;
    if (!visible)
    {
        window.state().visible = false;
        return;
    }

    const bool drawContent = window.begin(displayWidth, displayHeight);
    visible = window.state().visible;
    if (!visible)
    {
        window.end();
        return;
    }
    if (!drawContent)
    {
        window.end();
        return;
    }

    ImGui::Text("Level %u", snapshot_.characterLevel);
    ImGui::Text("XP: %u / next %u", snapshot_.characterExperience, snapshot_.experienceToNextLevel);
    ImGui::Separator();

    if (!recentSkillUps_.empty())
    {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Recent skill-ups:");
        for (const auto& skillId : recentSkillUps_)
        {
            std::string label = skillId;
            if (skillCatalog_ != nullptr)
            {
                if (const tbeq::SkillDef* skill = skillCatalog_->findSkill(skillId))
                {
                    label = skill->displayName;
                }
            }
            ImGui::BulletText("%s", label.c_str());
        }
        ImGui::Separator();
    }

    if (ImGui::BeginTable("SkillsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable))
    {
        ImGui::TableSetupColumn("Skill");
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Level");
        ImGui::TableSetupColumn("Cap");
        ImGui::TableHeadersRow();

        std::vector<net::SkillEntryPayload> rows;
        rows.reserve(skillEntries_.size());
        for (const auto& [_, entry] : skillEntries_)
        {
            rows.push_back(entry);
        }
        std::sort(rows.begin(), rows.end(), [this](const net::SkillEntryPayload& a, const net::SkillEntryPayload& b)
        {
            tbeq::SkillCategory catA = tbeq::SkillCategory::MeleeWeapon;
            tbeq::SkillCategory catB = tbeq::SkillCategory::MeleeWeapon;
            if (skillCatalog_ != nullptr)
            {
                if (const tbeq::SkillDef* skillA = skillCatalog_->findSkill(a.skillId))
                {
                    catA = skillA->category;
                }
                if (const tbeq::SkillDef* skillB = skillCatalog_->findSkill(b.skillId))
                {
                    catB = skillB->category;
                }
            }
            if (catA != catB)
            {
                return catA < catB;
            }
            return a.skillId < b.skillId;
        });

        for (const auto& entry : rows)
        {
            std::string displayName = entry.skillId;
            std::string category = "Unknown";
            std::string description;
            tbeq::SkillCategory skillCategory = tbeq::SkillCategory::MeleeWeapon;
            if (skillCatalog_ != nullptr)
            {
                if (const tbeq::SkillDef* skill = skillCatalog_->findSkill(entry.skillId))
                {
                    displayName = skill->displayName;
                    skillCategory = skill->category;
                    category = categoryLabel(skillCategory);
                    description = skill->description;
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool recent = std::find(recentSkillUps_.begin(), recentSkillUps_.end(), entry.skillId) != recentSkillUps_.end();
            if (recent)
            {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", displayName.c_str());
            }
            else
            {
                ImGui::TextUnformatted(displayName.c_str());
            }
            if (!description.empty() && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", description.c_str());
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(category.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", entry.level);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", entry.cap);
        }
        ImGui::EndTable();
    }

    window.end();
}

} // namespace tbeq::client
