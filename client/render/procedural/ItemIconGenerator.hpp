#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tbeq::render
{

class ItemIconGenerator
{
public:
    static constexpr int kIconSize = 16;

    std::vector<uint8_t> generate(
        const std::string& iconShape,
        const std::string& tintHex) const;
};

} // namespace tbeq::render
