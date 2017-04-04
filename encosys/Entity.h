#pragma once

#include "EcsTypes.h"
#include <functional>

namespace ECS {

class EntityManager;

class Entity {
public:
    Entity () = default;
    explicit operator uint64_t () const { return m_id; }
    uint64_t Id () const { return m_id; }

    friend bool operator== (const Entity& lhs, const Entity& rhs) { return lhs.m_id == rhs.m_id; }
    friend bool operator!= (const Entity& lhs, const Entity& rhs) { return lhs.m_id != rhs.m_id; }
    friend bool operator<  (const Entity& lhs, const Entity& rhs) { return lhs.m_id < rhs.m_id; }
    friend bool operator>  (const Entity& lhs, const Entity& rhs) { return lhs.m_id > rhs.m_id; }
    friend bool operator<= (const Entity& lhs, const Entity& rhs) { return lhs.m_id <= rhs.m_id; }
    friend bool operator>= (const Entity& lhs, const Entity& rhs) { return lhs.m_id >= rhs.m_id; }

private:
    friend class EntityManager;
    explicit Entity (uint64_t id) : m_id{id} {}
    uint64_t m_id{static_cast<uint64_t>(-1)};
};

const Entity c_invalidEntity = {};

}

namespace std {
    template <>
    struct hash<ECS::Entity> {
        std::size_t operator() (const ECS::Entity& k) const {
            return std::hash<uint64_t>()(k.Id());
        }
    };
}
