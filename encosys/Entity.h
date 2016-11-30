#pragma once

#include "EncosysTypes.h"

namespace ECS {

template <uint32_t> class EntityManager;

class Entity {
public:
    Entity () = default;
    operator uint64_t () const { return m_id; }

    bool operator== (const Entity& other) const { return m_id == other.m_id; }
    bool operator!= (const Entity& other) const { return m_id != other.m_id; }
    bool operator<  (const Entity& other) const { return m_id < other.m_id; }
    bool operator>  (const Entity& other) const { return m_id > other.m_id; }
    bool operator<= (const Entity& other) const { return m_id <= other.m_id; }
    bool operator>= (const Entity& other) const { return m_id >= other.m_id; }

private:
    template <uint32_t> friend class EntityManager;
    explicit Entity (uint64_t id) : m_id{id} {}
    uint64_t m_id{static_cast<uint64_t>(-1)};
};

const Entity c_invalidEntity = {};

}

namespace std {
    template <>
    struct hash<ECS::Entity> {
        std::size_t operator() (const ECS::Entity& k) const {
            return std::hash<uint64_t>()(k);
        }
    };
}
