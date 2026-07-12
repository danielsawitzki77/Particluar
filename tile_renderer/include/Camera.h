#pragma once

#include <SDL3/SDL.h>

class Camera {
public:
    Camera();

    void SetPosition(float x, float y);
    float GetX() const;
    float GetY() const;

    // Pivot: normalized 0.0-1.0 per axis, clamped. Default (0.5, 0.5).
    void SetPivot(float px, float py);
    float GetPivotX() const;
    float GetPivotY() const;

    // Moves camera based on held WASD keys. deltaTime in seconds.
    // keyState is SDL_GetKeyboardState pointer, indexed by SDL_SCANCODE_W/A/S/D.
    void Update(float deltaTime, float scrollSpeed, const bool* keyState);

private:
    float m_x, m_y;
    float m_pivot_x, m_pivot_y;
};
