// SPDX-License-Identifier: MIT
#include "export/CSharpSources.h"
#include "io/watch/LinkedFolders.h"
#include "io/watch/SharedFileReader.h"
#include "util/FileUtils.h"
#include "util/SafeWrite.h"
#include "util/Sha256.h"
#include "json.hpp"

#include <sstream>

namespace jasperengine::editor::exportfiles {
namespace {
    namespace fs = std::filesystem;
    using Json = nlohmann::json;
    const fs::path csharpDir = fs::path("project") / "csharp";
    const fs::path csharpManifest = csharpDir / ".allio-exported-sources.json";
    const fs::path csharpProject = csharpDir / "Allio.Game.csproj";

    bool plainSourcePath(const fs::path& path) {
        if (path.empty() || path.has_root_path()) return false;
        for (const auto& part : path) {
            if (part == "." || part == ".." || part.empty() || part.native().find(':') != fs::path::string_type::npos)
                return false;
        }
        const auto ext = path.extension().native();
        return ext.size() == 3 && ext[0] == '.' && (ext[1] == 'c' || ext[1] == 'C') && (ext[2] == 's' || ext[2] == 'S');
    }

    std::string csharpProjectSkeleton(const std::string& text) {
        // Pre-manifest exports have the same project XML, with one Compile item per copied script.
        // Retire the project file only when every other byte is the generator's output.
        std::istringstream input(text);
        std::string stripped;
        for (std::string line; std::getline(input, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("    <Compile Include=\"src/") && line.ends_with("\" />")) continue;
            stripped += line + "\n";
        }
        return stripped;
    }

    bool linkedSourcePath(const fs::path& root, const fs::path& relative) {
        fs::path path = root;
        for (const auto& part : relative) {
            path /= part;
            if (watch::isLink(path)) return true;
        }
        return false;
    }
}

bool updateCSharpSources(const fs::path& exportRoot, const fs::path& sourceRoot,
                         const std::vector<fs::path>& files, const std::string& projectXml, std::string& error) {
    const fs::path scripts = csharpDir / "src";
    if (linkedSourcePath(exportRoot, csharpManifest) || linkedSourcePath(exportRoot, csharpProject)) {
        error = "The exported C# project or its source manifest is a linked path.";
        return false;
    }

    Json previous = Json::object();
    std::error_code ec;
    const bool hasManifest = fs::exists(exportRoot / csharpManifest, ec);
    if (ec) { error = ec.message(); return false; }
    if (hasManifest) {
        std::string bytes;
        if (SharedFileReader::readAll(exportRoot / csharpManifest, bytes))
            previous = Json::parse(bytes, nullptr, false);
        if (previous.is_discarded() || !previous.is_object() || !previous.contains("sources") ||
            !previous["sources"].is_object() || !previous.contains("version") || previous["version"] != 1) {
            error = "Cannot read the AllioEngine C# source manifest in " + FileUtils::pathToUtf8(exportRoot / csharpDir);
            return false;
        }
    }

    Json sources = Json::object();
    const Json previousSources = hasManifest ? previous["sources"] : Json::object();
    std::string existingProject;
    const bool hasProject = fs::is_regular_file(exportRoot / csharpProject, ec);
    if (hasProject && !SharedFileReader::readAll(exportRoot / csharpProject, existingProject)) {
        error = "Cannot read exported C# project " + FileUtils::pathToUtf8(exportRoot / csharpProject);
        return false;
    }
    const bool matchingProject = hasProject && existingProject == projectXml;
    const bool ownedProject = hasProject && (hasManifest
        ? previous.contains("project") && previous["project"] == sha256Hex(existingProject)
        : existingProject.find("StageAllioHostConfig") != std::string::npos &&
          csharpProjectSkeleton(existingProject) == csharpProjectSkeleton(projectXml));
    if (!files.empty() && hasProject && !matchingProject && !ownedProject) {
        error = "The exported C# project has local changes: " + FileUtils::pathToUtf8(exportRoot / csharpProject);
        return false;
    }
    for (const auto& file : files) {
        if (!plainSourcePath(file) || linkedSourcePath(sourceRoot, file) || linkedSourcePath(exportRoot, scripts / file)) {
            error = "Invalid or linked C# export path: " + FileUtils::pathToUtf8(file);
            return false;
        }
        std::string bytes;
        if (!SharedFileReader::readAll(sourceRoot / file, bytes)) {
            error = "Cannot read C# script: " + FileUtils::pathToUtf8(sourceRoot / file);
            return false;
        }
        const std::string name = FileUtils::pathToGenericUtf8(file);
        const std::string digest = sha256Hex(bytes);
        const fs::path target = exportRoot / scripts / file;
        if (fs::exists(target, ec)) {
            std::string actual;
            if (!sha256File(target, actual, &error)) return false;
            if (actual == digest) {
                // A matching legacy project lets us adopt a verified copy from before manifests.
                if (previousSources.contains(name) || (!hasManifest && matchingProject))
                    sources[name] = digest;
                continue;
            }
            if (!previousSources.contains(name) || previousSources[name] != actual) {
                error = "The exported C# file has local changes. Move or merge it before exporting: " +
                        FileUtils::pathToUtf8(target);
                return false;
            }
        } else if (ec) { error = ec.message(); return false; }
        if (!safewrite::writeFileBeneath(exportRoot, scripts / file, bytes, error)) return false;
        sources[name] = digest;
    }

    if (!files.empty() && !matchingProject && !safewrite::writeFileBeneath(exportRoot, csharpProject, projectXml, error)) return false;

    if (hasManifest) {
        for (const auto& [name, digest] : previousSources.items()) {
            if (sources.contains(name) || !digest.is_string()) continue;
            const fs::path file = FileUtils::pathFromUtf8(name);
            if (!plainSourcePath(file) || linkedSourcePath(exportRoot, scripts / file)) continue;
            const fs::path target = exportRoot / scripts / file;
            if (!fs::is_regular_file(target, ec)) continue;
            // On a case-insensitive volume, a differently spelled active path can name this same file.
            bool stillActive = false;
            for (const auto& active : files) {
                if (fs::equivalent(target, exportRoot / scripts / active, ec)) { stillActive = true; break; }
            }
            if (stillActive) continue;
            std::string actual;
            if (!sha256File(target, actual, &error)) return false;
            if (digest == actual && !safewrite::removeFileBeneath(exportRoot, scripts / file, error)) return false;
        }
    }

    if (files.empty()) {
        if (ownedProject && !safewrite::removeFileBeneath(exportRoot, csharpProject, error)) return false;
        return !hasManifest || safewrite::removeFileBeneath(exportRoot, csharpManifest, error);
    }

    const Json manifest = {{"version", 1}, {"project", sha256Hex(projectXml)}, {"sources", std::move(sources)}};
    return safewrite::writeFileBeneath(exportRoot, csharpManifest, manifest.dump(2) + "\n", error);
}
}
