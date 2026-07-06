
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>

struct Station
{
    std::string name;
    float fuel;
    float depleteRate;

    glm::vec2 pos;
};

struct WeatherCell
{
    float weight;
};

struct Vessel {
    glm::vec2 pos;
    float speed;
    float fuel;
    float maxFuel;
    Station* target = nullptr;

    float getSpeedModifier(const WeatherCell& w) {
        return 1.0f / (1.0f + w.weight); // heavier weather = slower
    }
};

class ServerNav {
public:
    std::vector<Station> stations;
    std::vector<Vessel> vessels;
    WeatherCell map[50][50];

    void update(float dt);
};