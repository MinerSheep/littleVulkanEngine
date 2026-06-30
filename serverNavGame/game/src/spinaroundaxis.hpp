#pragma once

#include "game_object.hpp"

struct SpinAroundAxis : public Component {
    glm::vec3 center;
    float radius;
    float speed;
    float angle = 0.f;
    glm::vec3 axis;

    TransformComponent* transform = nullptr;

    SpinAroundAxis(TransformComponent* transform, glm::vec3 center, float radius, float speed, glm::vec3 axis = {0,1,0})
        : transform(transform), center(center), radius(radius), speed(speed), axis(axis) {}

    void update(float dt, GameObject& obj) override {
        if (!transform) return;

        angle += speed * dt;

        // Project rotation onto the axis plane
        glm::vec3 offset;
        offset.x = cos(angle) * radius;
        offset.z = sin(angle) * radius;
        offset.y = 0.f;

        // Apply axis rotation
        glm::mat4 rot = glm::rotate(glm::mat4(1.f), angle, axis);
        glm::vec3 rotated = glm::vec3(rot * glm::vec4(offset, 1.f));

        transform->translation = center + rotated;
    }
};
