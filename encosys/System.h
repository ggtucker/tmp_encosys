#pragma once

#include "Entity.h"

namespace ECS {

template <typename ComponentList> class EntityManager;

class System {
public:
    virtual void PreUpdate () {}
    virtual void PostUpdate () {}

protected:
    float DeltaTime () const { return m_deltaTime; }

    template <typename EntityManager>
    EntityManager* EntityManager () { return static_cast<EntityManager*>(m_entityManager); }

private:
    template <typename ComponentList> friend class EntityManager;
    void* m_entityManager{};
    float m_deltaTime{};
};

}