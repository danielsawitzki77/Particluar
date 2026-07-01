#include "ModelSwitcher.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace BodyRenderer {

bool ModelSwitcher::LoadDirectory(const std::string& dir_path)
{
    m_paths.clear();
    m_current_index = 0;

    // Use SDL3 filesystem API to enumerate directory
    int count = 0;
    char** files = SDL_GlobDirectory(dir_path.c_str(), "*.json", 0, &count);
    if (!files) {
        SDL_Log("[ModelSwitcher] Cannot read directory: %s", dir_path.c_str());
        return false;
    }

    for (int i = 0; i < count; ++i) {
        std::string full_path = dir_path;
        if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\') {
            full_path += '/';
        }
        full_path += files[i];
        m_paths.push_back(full_path);
    }

    SDL_free(files);

    // Sort alphabetically (case-insensitive)
    std::sort(m_paths.begin(), m_paths.end(), [](const std::string& a, const std::string& b) {
        std::string la = a, lb = b;
        std::transform(la.begin(), la.end(), la.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        std::transform(lb.begin(), lb.end(), lb.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return la < lb;
    });

    return !m_paths.empty();
}

int ModelSwitcher::GetCount() const
{
    return static_cast<int>(m_paths.size());
}

int ModelSwitcher::GetCurrentIndex() const
{
    return m_current_index;
}

std::string ModelSwitcher::GetCurrentPath() const
{
    if (m_paths.empty()) return "";
    return m_paths[m_current_index];
}

void ModelSwitcher::Next()
{
    if (m_paths.empty()) return;
    m_current_index = (m_current_index + 1) % static_cast<int>(m_paths.size());
}

void ModelSwitcher::Previous()
{
    if (m_paths.empty()) return;
    m_current_index = (m_current_index - 1 + static_cast<int>(m_paths.size())) % static_cast<int>(m_paths.size());
}

} // namespace BodyRenderer
