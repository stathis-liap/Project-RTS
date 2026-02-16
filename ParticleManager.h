#pragma once
#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "FountainEmitter.h"
#include "OrbitEmitter.h"

class ParticleManager {
public:
    static std::vector<std::unique_ptr<IntParticleEmitter>> active_emitters;
    static Drawable* particle_quad;

    static void init(Drawable* quad) {
        particle_quad = quad;
    }

    static void addExplosion(glm::vec3 pos) {
        auto explosion = std::make_unique<OrbitEmitter>(particle_quad, 100, 1.0f, 12.0f);
        explosion->emitter_pos = pos;
        explosion->max_duration = 0.8f;
        active_emitters.push_back(std::move(explosion));
    }

    static void addMageImpact(glm::vec3 pos) {
        auto impact = std::make_unique<FountainEmitter>(particle_quad, 30);
        impact->emitter_pos = pos;
        impact->max_duration = 0.5f;
        active_emitters.push_back(std::move(impact));
    }

    static void updateAndRender(float dt, glm::vec3 camera_pos, glm::mat4 PV, GLuint shader, GLuint texture) {
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader, "PV"), 1, GL_FALSE, &PV[0][0]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(shader, "texture0"), 0);

        auto it = active_emitters.begin();
        while (it != active_emitters.end()) {
            (*it)->playback_timer += dt;
            if ((*it)->playback_timer > (*it)->max_duration) {
                it = active_emitters.erase(it);
            }
            else {
                (*it)->updateParticles(0.0f, dt, camera_pos);
                (*it)->renderParticles();
                ++it;
            }
        }
    }

    static void addMageBeam(glm::vec3 start, glm::vec3 end) {
        // 60 particles per beam
        auto beam = std::make_unique<FountainEmitter>(particle_quad, 1);
        beam->emitter_pos = start;
        beam->target_pos = end; // You'll need to add this member to IntParticleEmitter
        beam->is_beam = true;    // A flag to tell the emitter how to spawn
        beam->max_duration = 0.2f;
        active_emitters.push_back(std::move(beam));
    }
};