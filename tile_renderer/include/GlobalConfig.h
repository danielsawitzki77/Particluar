#pragma once

#include <string>

struct GlobalConfigData {
    int tile_width;
    int tile_height;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    float scroll_speed;
};

class GlobalConfig {
public:
    // Loads from file. Returns true if file parsed successfully.
    // On any failure, falls back to hard-coded defaults and logs via SDL_Log.
    bool Load(const std::string& filepath);

    // Returns current config (either loaded or defaults).
    const GlobalConfigData& Get() const;

    // Serializes current config back to JSON string (round-trip safe).
    std::string Serialize() const;

private:
    GlobalConfigData m_data;
    void ApplyDefaults();
    bool Validate(const GlobalConfigData& candidate) const;
};
