#include "LightManager.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <SDL3/SDL.h>

namespace BodyRenderer {

static const int MAX_LIGHTS = 8;
static const GLenum GL_LIGHT_IDS[8] = {
    GL_LIGHT0, GL_LIGHT1, GL_LIGHT2, GL_LIGHT3,
    GL_LIGHT4, GL_LIGHT5, GL_LIGHT6, GL_LIGHT7
};

void LightManager::Apply(const std::vector<PointLight>& lights, const Vec3& ambient) const
{
    glEnable(GL_LIGHTING);

    // Set global ambient
    float amb[4] = { ambient.x, ambient.y, ambient.z, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);

    int count = static_cast<int>(lights.size());
    if (count > MAX_LIGHTS) {
        SDL_Log("[BodyRenderer] Warning: %d lights requested, capping at %d", count, MAX_LIGHTS);
        count = MAX_LIGHTS;
    }

    for (int i = 0; i < count; ++i) {
        GLenum light_id = GL_LIGHT_IDS[i];
        glEnable(light_id);

        float pos[4] = { lights[i].position.x, lights[i].position.y, lights[i].position.z, 1.0f };
        float diff[4] = { lights[i].diffuse.x, lights[i].diffuse.y, lights[i].diffuse.z, 1.0f };
        float spec[4] = { lights[i].specular.x, lights[i].specular.y, lights[i].specular.z, 1.0f };

        glLightfv(light_id, GL_POSITION, pos);
        glLightfv(light_id, GL_DIFFUSE, diff);
        glLightfv(light_id, GL_SPECULAR, spec);
        glLightf(light_id, GL_CONSTANT_ATTENUATION, lights[i].constant_atten);
        glLightf(light_id, GL_LINEAR_ATTENUATION, lights[i].linear_atten);
        glLightf(light_id, GL_QUADRATIC_ATTENUATION, lights[i].quadratic_atten);
    }

    // Disable unused lights
    for (int i = count; i < MAX_LIGHTS; ++i) {
        glDisable(GL_LIGHT_IDS[i]);
    }
}

void LightManager::Disable() const
{
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        glDisable(GL_LIGHT_IDS[i]);
    }
    glDisable(GL_LIGHTING);
}

} // namespace BodyRenderer
