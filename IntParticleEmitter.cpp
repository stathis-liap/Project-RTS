#include "IntParticleEmitter.h"
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

IntParticleEmitter::IntParticleEmitter(Drawable* _model, int number) {
    model = _model;
    number_of_particles = number;
    emitter_pos = glm::vec3(0.0f, 0.0f, 0.0f);

    p_attributes.resize(number_of_particles, particleAttributes());
    translations.resize(number_of_particles, glm::mat4(0.0f));
    rotations.resize(number_of_particles, glm::mat4(1.0f));
    scales.resize(number_of_particles, 1.0f);
    lifes.resize(number_of_particles, 0.0f);

    configureVAO();
}

void IntParticleEmitter::renderParticles(int time) {
    if (number_of_particles == 0) return;
    std::cout << "Rendering " << number_of_particles << " particles" << std::endl;
    bindAndUpdateBuffers();

    glBindVertexArray(emitterVAO);
    // Draw using instancing
    glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)(3 * model->indices.size()), GL_UNSIGNED_INT, 0, number_of_particles);
    glBindVertexArray(0);
}

void IntParticleEmitter::bindAndUpdateBuffers() {
    // Standard sorting (non-parallel to avoid compiler issues)
    if (use_sorting) {
        std::sort(p_attributes.begin(), p_attributes.end());
    }

    // Standard loops instead of std::transform/execution
    for (int i = 0; i < number_of_particles; i++) {
        const auto& p = p_attributes[i];

        if (p.life <= 0) {
            translations[i] = glm::mat4(0.0f);
            scales[i] = 0.0f;
            lifes[i] = 0.0f;
            continue;
        }

        translations[i] = glm::translate(glm::mat4(1.0f), p.position);

        if (use_rotations) {
            rotations[i] = glm::rotate(glm::mat4(1.0f), glm::radians(p.rot_angle), p.rot_axis);
        }
        else {
            rotations[i] = glm::mat4(1.0f);
        }

        scales[i] = p.mass;
        lifes[i] = p.life;
    }

    // Bind and send data to GPU
    glBindVertexArray(emitterVAO);

    // Position/Translation Buffer
    glBindBuffer(GL_ARRAY_BUFFER, transformations_buffer);
    glBufferData(GL_ARRAY_BUFFER, number_of_particles * sizeof(glm::mat4), translations.data(), GL_STREAM_DRAW);

    // Rotation Buffer
    glBindBuffer(GL_ARRAY_BUFFER, rotations_buffer);
    glBufferData(GL_ARRAY_BUFFER, number_of_particles * sizeof(glm::mat4), rotations.data(), GL_STREAM_DRAW);

    // Scale Buffer
    glBindBuffer(GL_ARRAY_BUFFER, scales_buffer);
    glBufferData(GL_ARRAY_BUFFER, number_of_particles * sizeof(float), scales.data(), GL_STREAM_DRAW);

    // Life Buffer
    glBindBuffer(GL_ARRAY_BUFFER, lifes_buffer);
    glBufferData(GL_ARRAY_BUFFER, number_of_particles * sizeof(float), lifes.data(), GL_STREAM_DRAW);
}

void IntParticleEmitter::changeParticleNumber(int new_number) {
    if (new_number == number_of_particles) return;

    number_of_particles = new_number;
    p_attributes.resize(number_of_particles, particleAttributes());
    translations.resize(number_of_particles, glm::mat4(0.0f));
    rotations.resize(number_of_particles, glm::mat4(1.0f));
    scales.resize(number_of_particles, 1.0f);
    lifes.resize(number_of_particles, 0.0f);
}

glm::vec4 IntParticleEmitter::calculateBillboardRotationMatrix(glm::vec3 particle_pos, glm::vec3 camera_pos)
{
    glm::vec3 dir = camera_pos - particle_pos;
    dir.y = 0; // Keep rotation on the Y axis for RTS feel
    if (glm::length(dir) > 0.0001f) dir = glm::normalize(dir);
    else dir = glm::vec3(0, 0, 1);

    glm::vec3 rot_axis = glm::cross(glm::vec3(0, 0, 1), dir);
    float dotProduct = glm::dot(glm::vec3(0, 0, 1), dir);
    float rot_angle = glm::acos(glm::clamp(dotProduct, -1.0f, 1.0f));

    return glm::vec4(rot_axis, rot_angle);
}

void IntParticleEmitter::configureVAO() {
    glGenVertexArrays(1, &emitterVAO);
    glBindVertexArray(emitterVAO);

    // Vertices
    glBindBuffer(GL_ARRAY_BUFFER, model->verticesVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    // Normals
    if (!model->indexedNormals.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, model->normalsVBO);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(1);
    }

    // UVs
    if (!model->indexedUVS.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, model->uvsVBO);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(2);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->elementVBO);

    std::size_t vec4Size = sizeof(glm::vec4);

    // Transformations (Attribs 3-6)
    glGenBuffers(1, &transformations_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, transformations_buffer);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(3 + i);
        glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)(4 * vec4Size), (void*)(i * vec4Size));
        glVertexAttribDivisor(3 + i, 1);
    }

    // Rotations (Attribs 7-10)
    glGenBuffers(1, &rotations_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, rotations_buffer);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(7 + i);
        glVertexAttribPointer(7 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)(4 * vec4Size), (void*)(i * vec4Size));
        glVertexAttribDivisor(7 + i, 1);
    }

    // Scales (Attrib 11)
    glGenBuffers(1, &scales_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, scales_buffer);
    glEnableVertexAttribArray(11);
    glVertexAttribPointer(11, 1, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribDivisor(11, 1);

    // Lifes (Attrib 12)
    glGenBuffers(1, &lifes_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, lifes_buffer);
    glEnableVertexAttribArray(12);
    glVertexAttribPointer(12, 1, GL_FLOAT, GL_FALSE, 0, NULL);
    glVertexAttribDivisor(12, 1);

    glBindVertexArray(0);
}