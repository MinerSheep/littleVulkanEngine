#include "servernav_sim.hpp"

void ServerNav::update(float dt) 
{
    // update stations
    for (auto& s : stations) {
        s.fuel -= s.depleteRate * dt;
        s.fuel = std::max(0.f, s.fuel);
    }

    for (auto& v : vessels) {
        if (v.target == nullptr)
        {
            // pick new target
            Station* newtar = nullptr;
            float min = 1.f;
            for (auto& s : stations) {
                if (s.fuel < min)
                {
                    newtar = &s;
                    min = s.fuel;
                }
            }

            if (newtar == nullptr)
                break;
            
            v.target = newtar;
        }

        glm::vec2 dir = glm::normalize(v.target->pos - v.pos);

        WeatherCell& w = map[(int)v.pos.x][(int)v.pos.y];
        float mod =  v.getSpeedModifier(w);

        v.pos += dir * v.speed * mod * dt;

        if ((v.pos - v.target->pos).length() < 1.f)
        {
            v.target->fuel = 1.f;
            v.fuel = 1.f;

            v.target = nullptr;
        }
    }
}