#pragma once

#include <filesystem>
#include <string>

namespace tbeq::test
{

class TempDatabase
{
public:
    TempDatabase();
    ~TempDatabase();

    TempDatabase(const TempDatabase&) = delete;
    TempDatabase& operator=(const TempDatabase&) = delete;

    const std::filesystem::path& path() const { return path_; }
    bool generateWorld(int64_t seed = 42) const;

private:
    std::filesystem::path path_;
};

} // namespace tbeq::test
