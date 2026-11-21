// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace jasperengine::editor::exportfiles {
    bool updateCSharpSources(const std::filesystem::path& exportRoot, const std::filesystem::path& sourceRoot,
                             const std::vector<std::filesystem::path>& files, const std::string& projectXml,
                             std::string& error);
}
