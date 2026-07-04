/**
 * @file AssetBrowser.cpp
 * @brief ImGui asset browser with drag&drop support
 *
 * PROJECT_RULES.md. Phase 8: Asset management + editor polish.
 */

#include "editor/AssetBrowser.h"
#include "core/LoggerMacros.h"

#include <imgui.h>

#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace kenga {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static std::string normalize_path(const std::string& p)
{
    std::string out = p;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

static std::string extension_lower(const std::string& filename)
{
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return {};
    std::string ext = filename.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

const char* AssetBrowser::icon_for_extension(const std::string& ext)
{
    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx")
        return "[3D] ";
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
        return "[Tex] ";
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
        return "[Snd] ";
    if (ext == ".json")
        return "[Pre] ";
    if (ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".glsl")
        return "[Shd] ";
    if (ext == ".lua")
        return "[Scr] ";
    return "[???] ";
}

// ---------------------------------------------------------------------------
// draw single file
// ---------------------------------------------------------------------------

void AssetBrowser::draw_file_entry(const std::string& file_path, const std::string& filename)
{
    const std::string ext = extension_lower(filename);
    const char* icon = icon_for_extension(ext);

    const std::string label = std::string(icon) + filename;
    const bool is_selected = (m_selected_asset == file_path);

    if (ImGui::Selectable(label.c_str(), is_selected)) {
        m_selected_asset = file_path;
    }

    // Drag source — any supported asset type
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        const std::string normalized = normalize_path(file_path);
        // Store the path as payload (include null terminator)
        ImGui::SetDragDropPayload("ASSET_PATH", normalized.c_str(),
                                  normalized.size() + 1);
        ImGui::Text("Drop: %s", filename.c_str());
        ImGui::EndDragDropSource();
    }

    // Tooltip with full path
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", normalize_path(file_path).c_str());
        ImGui::EndTooltip();
    }
}

// ---------------------------------------------------------------------------
// draw directory tree (recursive)
// ---------------------------------------------------------------------------

void AssetBrowser::draw_directory(const std::string& dir_path)
{
    // Collect and sort entries
    std::vector<fs::directory_entry> dirs;
    std::vector<fs::directory_entry> files;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        if (ec) break;
        if (entry.is_directory(ec)) {
            dirs.push_back(entry);
        } else if (entry.is_regular_file(ec)) {
            files.push_back(entry);
        }
    }

    // Sort alphabetically
    auto by_name = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() < b.path().filename().string();
    };
    std::sort(dirs.begin(), dirs.end(), by_name);
    std::sort(files.begin(), files.end(), by_name);

    // Directories first
    for (const auto& d : dirs) {
        const std::string name = d.path().filename().string();
        if (ImGui::TreeNode(name.c_str())) {
            draw_directory(d.path().string());
            ImGui::TreePop();
        }
    }

    // Then files
    for (const auto& f : files) {
        const std::string name = f.path().filename().string();
        draw_file_entry(f.path().string(), name);
    }
}

// ---------------------------------------------------------------------------
// main draw
// ---------------------------------------------------------------------------

void AssetBrowser::draw(Registry& /*registry*/, const std::string& root_path)
{
    ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_NoCollapse);

    // Try several candidate paths (exe may run from different working dirs)
    std::string resolved_root;
    {
        const std::string candidates[] = {
            root_path,
            "Debug/" + root_path,
            "Release/" + root_path,
            "../" + root_path,
            "../../" + root_path,
        };
        std::error_code ec;
        for (const auto& c : candidates) {
            if (fs::exists(c, ec) && fs::is_directory(c, ec)) {
                resolved_root = c;
                break;
            }
        }
    }

    if (resolved_root.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                           "Directory not found: %s", root_path.c_str());
        ImGui::End();
        return;
    }

    ImGui::Text("Root: %s", resolved_root.c_str());
    ImGui::Separator();

    // Scrollable child region
    ImGui::BeginChild("AssetTree", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    draw_directory(resolved_root);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace kenga
