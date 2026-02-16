#include "OrbitEmitter.h"
#include <glm/gtc/matrix_transform.hpp>

OrbitEmitter::OrbitEmitter(Drawable* _model, int number, float _radius_min, float _radius_max)
    : IntParticleEmitter(_model, number), radius_min(_radius_min), radius_max(_radius_max)
{
    particle_radius.resize(number_of_particles, 0.0f);
    for (int i = 0; i < number_of_particles; i++) {
        createNewParticle(i);
    }
}

void OrbitEmitter::updateParticles(float time, float dt, glm::vec3 camera_pos) {
    // 1. Calculate how far through the explosion we are (0.0 to 1.0)
    float progress = playback_timer / max_duration;

    for (int i = 0; i < number_of_particles; i++) {
        auto& p = p_attributes[i];

        // 2. Linear expansion: Radius grows as time passes
        float current_r = particle_radius[i] * progress;

        // 3. Keep the "swirl" by updating the angle slightly
        p.rot_angle += dt * 5.0f;

        // 4. Set position relative to the ground (y = 0.5f)
        p.position = emitter_pos + glm::vec3(
            current_r * sin(p.rot_angle),
            0.5f,
            current_r * cos(p.rot_angle)
        );

        // 5. Fade out: Life goes from 1.0 to 0.0
        p.life = 1.0f - progress;

        // 6. Update billboard rotation so the spheres/quads face the camera
        glm::vec4 rot = calculateBillboardRotationMatrix(p.position, camera_pos);
        p.rot_axis = glm::vec3(rot.x, rot.y, rot.z);
        p.rot_angle = glm::degrees(rot.w);
    }
}

void OrbitEmitter::createNewParticle(int index) {
    particleAttributes& particle = p_attributes[index];

    // Max distance this specific particle will reach
    particle_radius[index] = ((float)rand() / (float)RAND_MAX) * (radius_max - radius_min) + radius_min;

    particle.rot_angle = (float)(rand() % 360);
    particle.rot_axis = glm::vec3(0, 1, 0);

    // Size of the sphere/quad
    particle.mass = 0.02f + (((float)rand() / (float)RAND_MAX) * 2.0f);
    particle.life = 1.0f;
}