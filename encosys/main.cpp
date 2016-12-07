#include "EntityManager.h"
#include <iostream>

struct PositionComponent {
    PositionComponent (float x, float y, float z) : x{ x }, y{ y }, z{ z } {}
    float x, y, z;
};

int main () {
    ECS::EntityManager<> manager;
    ECS::Entity entity1 = manager.Create();
    manager.AddComponent<PositionComponent>(entity1, 10.0f, 20.0f, 30.0f);

    ECS::Entity entity2 = manager.Create();
    manager.AddComponent<PositionComponent>(entity2, 500.0f, 1000.0f, 2000.0f);

    ECS::Entity entity3 = manager.Create();

    for (uint32_t i = 0; i < 10; ++i) {
        manager.ForEach([](ECS::Entity entity, PositionComponent& position) {
            std::cout << entity.Id() << " " << position.x << " " << position.y << " " << position.z << std::endl;
            position.x += 1.0f;
        });
    }

    manager.Destroy(entity1);
    manager.Destroy(entity2);
    manager.Destroy(entity3);
}