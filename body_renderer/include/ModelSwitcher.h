#pragma once

#include <string>
#include <vector>

namespace BodyRenderer {

class ModelSwitcher {
public:
    bool LoadDirectory(const std::string& dir_path);
    int GetCount() const;
    int GetCurrentIndex() const;
    std::string GetCurrentPath() const;

    void Next();
    void Previous();

private:
    std::vector<std::string> m_paths;
    int m_current_index = 0;
};

} // namespace BodyRenderer
