// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace jasperengine::editor::csharp {
    namespace fs = std::filesystem;
    std::vector<fs::path> sources(const fs::path& root, std::string& error);
    std::string projectContents(const fs::path& sourceRoot, const std::vector<fs::path>& files,
                                const fs::path& apiProject, bool recordDiagnostics = false);
    bool writeProject(const fs::path& directory, const fs::path& sourceRoot,
                      const std::vector<fs::path>& files, const fs::path& apiProject,
                      bool recordDiagnostics = false);
    bool buildForPlay(const fs::path& root, const fs::path& cache, std::string& error);
}
