#include "Camera.h"

Camera::Camera()
    : m_x(0.0f)
    , m_y(0.0f)
    , m_pivot_x(0.5f)
    , m_pivot_y(0.5f)
{
}

void Camera::SetPosition(float x, float y)
{
    m_x = x;
    m_y = y;
}

float Camera::GetX() const
{
    return m_x;
}

float Camera::GetY() const
{
    return m_y;
}

void Camera::SetPivot(float px, float py)
{
    // Clamp to 0.0 - 1.0
    if (px < 0.0f) px = 0.0f;
    if (px > 1.0f) px = 1.0f;
    if (py < 0.0f) py = 0.0f;
    if (py > 1.0f) py = 1.0f;

    m_pivot_x = px;
    m_pivot_y = py;
}

float Camera::GetPivotX() const
{
    return m_pivot_x;
}

float Camera::GetPivotY() const
{
    return m_pivot_y;
}

void Camera::Update(float deltaTime, float scrollSpeed, const bool* keyState)
{
    if (!keyState) {
        return;
    }

    float movement = scrollSpeed * deltaTime;

    // W = move up (decrease Y)
    if (keyState[SDL_SCANCODE_W]) {
        m_y -= movement;
    }
    // S = move down (increase Y)
    if (keyState[SDL_SCANCODE_S]) {
        m_y += movement;
    }
    // A = move left (decrease X)
    if (keyState[SDL_SCANCODE_A]) {
        m_x -= movement;
    }
    // D = move right (increase X)
    if (keyState[SDL_SCANCODE_D]) {
        m_x += movement;
    }
}
