#pragma once

#include "Entity.h"

namespace ECS {

class EntityManager;

class CSystem {
public:
    virtual void PreUpdate () {}
    virtual void PostUpdate () {}

protected:
    EntityManager* Manager () const { return m_manager; }
    float DeltaTime () const { return m_deltaTime; }

private:
    friend class EntityManager;
    EntityManager* m_manager{};
    float m_deltaTime{};
};

}