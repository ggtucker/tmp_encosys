#include "EntityManager.h"

struct PositionComponent {
    PositionComponent (float x, float y, float z) : x{ x }, y{ y }, z{ z } {}
    float x, y, z;
};

int main () {
    ECS::EntityManager<> manager;
    ECS::Entity entity = manager.Create();
    manager.AddComponent<PositionComponent>(entity, 10.0f, 20.0f, 30.0f);

    for (uint32_t i = 0; i < 10; ++i) {
        manager.ForEach([](ECS::Entity entity, PositionComponent& position) {
            std::cout << entity << " " << position.x << " " << position.y << " " << position.z << std::endl;
            position.x += 1.0f;
        });
    }

    manager.Destroy(entity);
}