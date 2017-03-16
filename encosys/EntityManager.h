#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "BlockObjectPool.h"
#include "Entity.h"
#include "FunctionTraits.h"

#ifndef ECS_MAX_COMPONENTS_
#define ECS_MAX_COMPONENTS_ 64
#endif

namespace ECS {

class EntityManager {
private:
    using ComponentMask = std::bitset<ECS_MAX_COMPONENTS_>;
    using ComponentIndexCard = std::array<uint32_t, ECS_MAX_COMPONENTS_>;

public:
    EntityManager () = default;
    virtual ~EntityManager ();

    Entity Create (bool active = true);
    void Destroy (const Entity& id);
    bool IsActive (const Entity& id) const;
    void SetActive (const Entity& id, bool active);
    bool IsValid (const Entity& id) const;

    template <typename TComponent, typename... TArgs>
    void AddComponent (const Entity& id, TArgs&&... args) {
        ECS_ASSERT_(IsValid(id));
        auto index = m_entityMap[id];
        auto& mask = m_componentMasks[index];
        auto& indexCard = m_componentIndexCards[index];
        auto componentIndex = GetComponentIndex<TComponent>();
        ECS_ASSERT_(mask.test(componentIndex) == false);
        auto& pool = GetComponentPool<TComponent>();
        indexCard[componentIndex] = pool.Create(std::forward<TArgs>(args)...);
        mask.set(componentIndex);
    }

    template <typename TComponent>
    void RemoveComponent (const Entity& id) {
        ECS_ASSERT_(IsValid(id));
        auto index = m_entityMap[id];
        auto& mask = m_componentMasks[index];
        auto& indexCard = m_componentIndexCards[index];
        auto componentIndex = GetComponentIndex<TComponent>();
        ECS_ASSERT_(mask.test(componentIndex) == true);
        auto& pool = GetComponentPool<TComponent>();
        pool.Destroy(indexCard[componentIndex]);
        mask.set(componentIndex, false);
    }

    template <typename TFunc>
    void ForEach (TFunc&& callback) {
        using FTraits = FunctionTraits<decltype(callback)>;
        static_assert(FTraits::ArgCount > 0, "First callback param must be ECS::Entity.");
        static_assert(std::is_same<FTraits::Arg<0>, Entity>::value, "First callback param must be ECS::Entity.");
        using FComponentArgs = typename FTraits::Args::RemoveFirst;

        ComponentMask targetMask{};
        FComponentArgs::ForTypes([&targetMask] (auto t) {
            targetMask.set(GetComponentIndex<TYPE_OF(t)>());
        });

        for (uint32_t i = 0; i < m_entityActiveCount; ++i) {
            if ((targetMask & m_componentMasks[i]) == targetMask) {
                UnpackAndCallback(
                    i,
                    callback,
                    FComponentArgs{},
                    typename GenerateSequence<FTraits::ArgCount>::Type{}
                );
            }
        }
    }

private:
    // m_entityIdCounter is the id to be used for the next entity created.
    uint64_t m_entityIdCounter{0};

    // m_entityMap is used to map entities to the index of their component mask.
    std::unordered_map<Entity, uint32_t> m_entityMap{};

    // m_componentMasks contains the bit masks representing which components an entity has.
    std::vector<ComponentMask> m_componentMasks{};

    // m_entities contains the id corresponding to the component mask at the same index.
    std::vector<Entity> m_entities{};

    // m_componentIndexCards contains the indices of components for each entity.
    std::vector<ComponentIndexCard> m_componentIndexCards{};

    // m_componentPools contains the memory pools for each component type.
    std::vector<std::unique_ptr<BlockMemoryPool>> m_componentPools{};

    // m_entityActiveCount is the number of entities that are currently active.
    // Active entities are packed together at the beginning of the m_componentMasks array.
    uint32_t m_entityActiveCount{0};

private:
    // Component indexing
    static uint32_t s_componentIndexCounter;
    template <typename Component>
    static uint32_t GetComponentIndex () {
        return _GetComponentIndexImpl<std::decay_t<Component>>();
    }

    template <typename Component>
    static uint32_t _GetComponentIndexImpl () {
        static auto index = s_componentIndexCounter++;
        ECS_ASSERT_(index < ECS_MAX_COMPONENTS_);
        return index;
    }

    // Component pools
    template <typename Component>
    BlockObjectPool<std::decay_t<Component>>& GetComponentPool () {
        auto componentIndex = GetComponentIndex<Component>();
        if (componentIndex >= m_componentPools.size()) {
            m_componentPools.resize(componentIndex + 1);
        }
        if (m_componentPools[componentIndex] == nullptr) {
            m_componentPools[componentIndex].reset(new BlockObjectPool<std::decay_t<Component>>());
        }
        return *static_cast<BlockObjectPool<std::decay_t<Component>>*>(
            m_componentPools[componentIndex].get()
        );
    }

    // ForEach helper
    template <typename TFunc, typename... Args, std::size_t... Seq>
    void UnpackAndCallback (uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>) {
        auto& indexCard = m_componentIndexCards[entityIndex];
        auto params = std::make_tuple(
            m_entities[entityIndex],
            std::ref(GetComponentPool<Args>().GetObject(indexCard[GetComponentIndex<Args>()]))...
        );
        callback(std::get<Seq>(params)...);
    }

    // Index helpers
    bool IndexIsActive (uint32_t index) const;
    void IndexSetActive (uint32_t& index, bool active);

    // Other helpers
    void SwapEntities (uint32_t lhsIndex, uint32_t rhsIndex);
};

}