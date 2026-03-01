#include "FountainEmitter.h"
#include <iostream>
#include <algorithm>

FountainEmitter::FountainEmitter(Drawable *_model, int number) : IntParticleEmitter(_model, number) {}

void FountainEmitter::updateParticles(float time, float dt, glm::vec3 camera_pos) {

    //This is for the fountain to slowly increase the number of its particles to the max amount
    //instead of shooting all the particles at once
    if (active_particles < number_of_particles) {
        int batch = 30;
        int limit = std::min(number_of_particles - active_particles, batch);
        for (int i = 0; i < limit; i++) {
            createNewParticle(active_particles);
            active_particles++;
        }
    }
    else {
        active_particles = number_of_particles; //In case we resized our ermitter to a smaller particle number
    }

    for(int i = 0; i < active_particles; i++){
        particleAttributes & particle = p_attributes[i];

        if(particle.position.y < emitter_pos.y - 10.0f || particle.life == 0.0f || checkForCollision(particle)){
            createNewParticle(i);
        }

        if (particle.position.y > height_threshold)
            createNewParticle(i);

        particle.accel = glm::vec3(-particle.position.x, 0.0f, -particle.position.z); //gravity force

        //particle.rot_angle += 90*dt; 

        particle.position = particle.position + particle.velocity*dt + particle.accel*(dt*dt)*0.5f;
        particle.velocity = particle.velocity + particle.accel*dt;

        //*
        auto bill_rot = calculateBillboardRotationMatrix(particle.position, camera_pos);
        particle.rot_axis = glm::vec3(bill_rot.x, bill_rot.y, bill_rot.z);
        particle.rot_angle = glm::degrees(bill_rot.w);
        //*/
        //particle.dist_from_camera = length(particle.position - camera_pos);
        particle.life = (height_threshold - particle.position.y) / (height_threshold - emitter_pos.y);
    }
}

bool FountainEmitter::checkForCollision(particleAttributes& particle)
{
    return particle.position.y < 0.0f;
}


void FountainEmitter::createNewParticle(int index) {
    auto& p = p_attributes[index];

    if (is_beam) {
        // 1. Calculate a random point along the line
        float lerpFactor = (float)rand() / (float)RAND_MAX;
        p.position = glm::mix(emitter_pos, target_pos, lerpFactor);

        // 2. Give it a little bit of "shiver" or jitter
        p.velocity = glm::vec3((RAND - 0.1f) * 5.0f, (RAND - 0.1f) * 5.0f, (RAND - 0.1f) * 5.0f);
        p.mass = 0.5f; // Small magic sparks
    }
    else {
        // ... your existing impact logic ...
        p.position = emitter_pos;
        p.velocity = glm::vec3((RAND - 0.5f) * 10.0f, RAND * 15.0f, (RAND - 0.5f) * 10.0f);
        p.mass = 2.0f;
    }

    p.life = 1.0f;
}
