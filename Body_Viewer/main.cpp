#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#include "BodyLoader.h"
#include "BodyRenderer.h"
#include "ConnectionSolver.h"
#include "ModelSwitcher.h"

#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct ViewerState {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 5.0f;
    BodyRenderer::Body current_body;
    bool has_model = false;
};

static void SetupProjection(int width, int height)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    gluPerspective(45.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

static void RenderFrame(const ViewerState& state, const BodyRenderer::BodyRendererGL& renderer)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!state.has_model) return;

    glLoadIdentity();

    // Camera: orbit around origin
    float cam_x = state.distance * std::sin(state.yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(state.pitch * static_cast<float>(M_PI) / 180.0f);
    float cam_y = state.distance * std::sin(state.pitch * static_cast<float>(M_PI) / 180.0f);
    float cam_z = state.distance * std::cos(state.yaw * static_cast<float>(M_PI) / 180.0f) * std::cos(state.pitch * static_cast<float>(M_PI) / 180.0f);

    gluLookAt(cam_x, cam_y, cam_z,
              0.0, 0.0, 0.0,
              0.0, 1.0, 0.0);

    // Render the body
    BodyRenderer::RenderParams params;
    params.ambient = state.current_body.material.ambient;
    params.shininess = state.current_body.material.shininess;
    params.model_color = state.current_body.root.color;

    // Default light at (5, 5, 5)
    BodyRenderer::PointLight light;
    light.position = BodyRenderer::Vec3(5.0f, 5.0f, 5.0f);
    light.diffuse = BodyRenderer::Vec3(1.0f, 1.0f, 1.0f);
    light.specular = BodyRenderer::Vec3(1.0f, 1.0f, 1.0f);
    params.lights.push_back(light);

    renderer.Render(state.current_body, params);
}

static bool LoadModel(const std::string& path, ViewerState& state)
{
    BodyRenderer::BodyLoader loader;
    BodyRenderer::LoadResult result = loader.LoadFromFile(path);
    if (!result.success) {
        SDL_Log("[Body_Viewer] Failed to load '%s': %s", path.c_str(), result.error.c_str());
        return false;
    }

    // Resolve connections
    BodyRenderer::ConnectionSolver solver;
    solver.ResolveTree(&result.body.root);

    state.current_body = result.body;
    state.has_model = true;
    return true;
}

int main(int argc, char* argv[])
{
    // Determine body directory
    std::string body_dir = "assets/bodies/";
    if (argc > 1) {
        body_dir = argv[1];
    }

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("[Body_Viewer] SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    SDL_Window* window = SDL_CreateWindow("Particluar Body Viewer", 800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("[Body_Viewer] Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create GL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_Log("[Body_Viewer] OpenGL context creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);

    // Setup initial projection
    SetupProjection(800, 600);

    // Load models from directory
    BodyRenderer::ModelSwitcher switcher;
    ViewerState state;
    BodyRenderer::BodyRendererGL renderer;

    if (switcher.LoadDirectory(body_dir)) {
        LoadModel(switcher.GetCurrentPath(), state);
        std::string title = "Particluar Body Viewer - " + switcher.GetCurrentPath();
        SDL_SetWindowTitle(window, title.c_str());
    } else {
        SDL_Log("[Body_Viewer] No models found in '%s'", body_dir.c_str());
    }

    // Main loop
    bool running = true;
    Uint64 last_time = SDL_GetTicks();
    const float ROTATION_SPEED = 90.0f; // degrees per second

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = static_cast<float>(now - last_time) / 1000.0f;
        last_time = now;

        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_LEFT) {
                    switcher.Previous();
                    LoadModel(switcher.GetCurrentPath(), state);
                    std::string title = "Particluar Body Viewer - " + switcher.GetCurrentPath();
                    SDL_SetWindowTitle(window, title.c_str());
                } else if (event.key.key == SDLK_RIGHT) {
                    switcher.Next();
                    LoadModel(switcher.GetCurrentPath(), state);
                    std::string title = "Particluar Body Viewer - " + switcher.GetCurrentPath();
                    SDL_SetWindowTitle(window, title.c_str());
                } else if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                int w = event.window.data1;
                int h = event.window.data2;
                SetupProjection(w, h);
                break;
            }
            }
        }

        // Continuous rotation via held keys
        const bool* keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_W]) state.pitch += ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_S]) state.pitch -= ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_A]) state.yaw -= ROTATION_SPEED * dt;
        if (keys[SDL_SCANCODE_D]) state.yaw += ROTATION_SPEED * dt;

        // Clamp pitch
        if (state.pitch > 89.0f) state.pitch = 89.0f;
        if (state.pitch < -89.0f) state.pitch = -89.0f;

        // Render
        RenderFrame(state, renderer);
        SDL_GL_SwapWindow(window);

        // Cap frame rate
        SDL_Delay(16); // ~60fps
    }

    // Cleanup
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
