#pragma once

#include <string>

struct GlobalConfigData {
    int tileWidth;
    int tileHeight;
    int viewportX;
    int viewportY;
    int viewportWidth;
    int viewportHeight;
    float scrollSpeed;
    int wfcMaxBacktracks;
    bool debugShowBlocking;
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
