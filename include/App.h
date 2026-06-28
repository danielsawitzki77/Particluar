#pragma once

// Particluar - Application header
// This file will contain the main application class declaration.

class App {
public:
    App() = default;
    ~App() = default;

    bool Init();
    void Run();
    void Shutdown();

private:
    bool m_running = false;
};
