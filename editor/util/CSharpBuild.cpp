// SPDX-License-Identifier: MIT
#include "util/CSharpBuild.h"
#include "io/watch/LinkedFolders.h"
#include "script/CSharpRuntime.h"
#include "util/CommandRunner.h"
#include "util/FileUtils.h"
#include "util/Util.h"

#include <algorithm>
#include <sstream>

namespace jasperengine::editor::csharp {

std::vector<fs::path> sources(const fs::path& root, std::string& error) {
    std::vector<fs::path> files;
    if (root.empty()) return files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        const auto name = FileUtils::pathToUtf8(entry.path().filename());
        if (watch::isLink(entry) || name.starts_with('.')) {
            it.disable_recursion_pending();
        } else if (entry.is_directory(ec)) {
            if (name == "build" || name == "bin" || name == "obj" || name == "node_modules"
                || fs::exists(entry.path() / "project.yaml", ec) || fs::exists(entry.path() / "CMakeCache.txt", ec)
                || fs::exists(entry.path() / "jasperengine.py", ec))
                it.disable_recursion_pending();
        } else if (Util::isCSharpFile(FileUtils::pathToUtf8(entry.path().extension())) && entry.is_regular_file(ec)) {
            files.push_back(entry.path().lexically_relative(root));
        }
        if (ec) break;
    }
    if (ec) error = "Cannot read the C# source folders in " + FileUtils::pathToUtf8(root) + ": " + ec.message();
    std::sort(files.begin(), files.end());
    return files;
}

// MSBuild expands item lists, wildcards and properties even in XML attributes.
static std::string itemPath(const fs::path& path) {
    std::string value;
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : FileUtils::pathToGenericUtf8(path)) {
        switch (c) {
            case '&': value += "&amp;"; break;
            case '<': value += "&lt;"; break;
            case '>': value += "&gt;"; break;
            case '"': value += "&quot;"; break;
            case '%': case '$': case '@': case ';': case '*': case '?': case '\'': case '(': case ')':
                value += '%'; value += hex[c >> 4]; value += hex[c & 15]; break;
            default: value += static_cast<char>(c);
        }
    }
    return value;
}

std::string projectContents(const fs::path& sourceRoot, const std::vector<fs::path>& files,
                            const fs::path& apiProject, bool recordDiagnostics) {
    std::ostringstream xml;
    xml << R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <EnableDefaultItems>false</EnableDefaultItems>
    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>
)";
    if (recordDiagnostics) xml << "    <ErrorLog>$(MSBuildProjectDirectory)/check.sarif,version=2.1</ErrorLog>\n";
    xml << R"(  </PropertyGroup>
  <ItemGroup>
)";
    xml << "    <ProjectReference Include=\"" << itemPath(apiProject) << "\" />\n";
    for (const auto& file : files)
        xml << "    <Compile Include=\"" << itemPath(sourceRoot / file) << "\" />\n";
    xml << R"(  </ItemGroup>
  <Target Name="StageAllioHostConfig" AfterTargets="Build">
    <Copy SourceFiles="$(TargetDir)Allio.Game.runtimeconfig.json"
          DestinationFiles="$(TargetDir)Allio.Managed.runtimeconfig.json" />
  </Target>
</Project>
)";
    return xml.str();
}

bool writeProject(const fs::path& directory, const fs::path& sourceRoot,
                  const std::vector<fs::path>& files, const fs::path& apiProject,
                  bool recordDiagnostics) {
    return FileUtils::writeFile(directory / "Allio.Game.csproj", projectContents(sourceRoot, files, apiProject, recordDiagnostics))
        != FileUtils::WriteResult::Failed;
}

bool buildForPlay(const fs::path& root, const fs::path& cache, std::string& error) {
    const auto files = sources(root, error);
    if (!error.empty()) return false;
    if (files.empty()) {
        CSharpRuntime::setAssemblyDirectory({});
        return true;
    }
    const auto projectDir = cache / "csharp";
    const auto api = FileUtils::getEngineDir() / "managed" / "Allio.Managed" / "Allio.Managed.csproj";
    if (!writeProject(projectDir, fs::absolute(root), files, api)) {
        error = "Cannot create the C# build project in " + FileUtils::pathToUtf8(projectDir);
        return false;
    }
    const auto output = projectDir / "managed";
    CommandRunner runner;
    if (!runner.run({"dotnet", "build", "Allio.Game.csproj", "--nologo", "-c", "Debug",
                     "--artifacts-path", FileUtils::pathToUtf8(projectDir / "artifacts"),
                     "-o", FileUtils::pathToUtf8(output)}, projectDir)) {
        error = "C# compilation failed. See the Build console and check that the .NET 10 SDK is on PATH.";
        return false;
    }
    if (!CSharpRuntime::verifyAssembly(output)) {
        error = "Cannot start the C# runtime or load Allio.Game.dll. Check the Console for details.";
        return false;
    }
    return true;
}

}
