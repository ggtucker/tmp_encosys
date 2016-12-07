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

namespace ECS {

template <uint32_t MaxComponents = 64>
class EntityManager {
private:
    using ComponentMask = std::bitset<MaxComponents>;
    using ComponentIndexCard = std::array<uint32_t, MaxComponents>;

public:
    EntityManager () = default;

    Entity Create (bool active = true) {
        Entity id{m_entityIdCounter};
        ++m_entityIdCounter;

        if (active) {
            if (m_entityActiveCount == m_entities.size()) {
                m_entityMap[id] = m_entities.size();
                m_componentMasks.push_back({});
                m_componentIndexCards.push_back({});
                m_entities.push_back(id);
            }
            else {
                m_entityMap[m_entities[m_entityActiveCount]] = m_entities.size();
                m_componentMasks.push_back(m_componentMasks[m_entityActiveCount]);
                m_componentIndexCards.push_back(m_componentIndexCards[m_entityActiveCount]);
                m_entities.push_back(m_entities[m_entityActiveCount]);

                m_entityMap[id] = m_entityActiveCount;
                m_componentMasks[m_entityActiveCount].reset();
                m_componentIndexCards[m_entityActiveCount] = {};
                m_entities[m_entityActiveCount] = id;
            }
            ++m_entityActiveCount;
        }
        else {
            m_entityMap[id] = m_entities.size();
            m_componentMasks.push_back({});
            m_componentIndexCards.push_back({});
            m_entities.push_back(id);
        }

        return id;
    }

    void Destroy (const Entity& id) {
        ECS_ASSERT_(IsValid(id));
        SetActive(id, false);
        auto index = m_entityMap[id];
        auto& mask = m_componentMasks[index];
        auto& indexCard = m_componentIndexCards[index];
        for (uint32_t i = 0; i < mask.size(); ++i) {
            if (mask.test(i)) {
                m_componentPools[i]->Destroy(indexCard[i]);
            }
        }
        mask.reset();
    }

    bool IsActive (const Entity& id) const {
        return IndexIsActive(m_entityMap[id])
    }

    void SetActive (const Entity& id, bool active) {
        auto index = m_entityMap[id];
        if (active == IndexIsActive(index)) {
            return;
        }
        if (m_entityActiveCount < m_entities.size()) {
            SwapEntities(index, m_entityActiveCount);
        }
        else {
            SwapEntities(index, m_entities.size() - 1);
        }
        m_entityActiveCount += (active ? 1 : -1);
    }

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

    bool IsValid (const Entity& id) const {
        return id != c_invalidEntity && m_entityMap.find(id) != m_entityMap.cend();
    }

    template <typename TFunc, typename... Args, std::size_t... Seq>
    void UnpackAndCallback (uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>) {
        auto& indexCard = m_componentIndexCards[entityIndex];
        auto params = std::make_tuple(
            m_entities[entityIndex],
            std::ref(GetComponentPool<Args>().GetObject(indexCard[GetComponentIndex<Args>()]))...
        );
        callback(std::get<Seq>(params)...);
    }

    template <typename TFunc>
    void ForEach (TFunc&& callback) {
        using FTraits = FunctionTraits<decltype(callback)>;
        static_assert(FTraits::ArgCount > 0, "First callback param must be an ECS::Entity.");
        static_assert(std::is_same<FTraits::Arg<0>, Entity>::value, "First callback param must be an ECS::Entity.");
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
    static uint32_t s_componentIndexCounter;
    template <typename Component>
    static uint32_t GetComponentIndex () {
        return _GetComponentIndexImpl<std::decay_t<Component>>();
    }

    template <typename Component>
    static uint32_t _GetComponentIndexImpl () {
        static auto index = s_componentIndexCounter++;
        ECS_ASSERT_(index < MaxComponents);
        return index;
    }

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

    bool IndexIsActive (uint32_t index) const {
        return index < m_entityActiveCount;
    }

    void SwapEntities (uint32_t lhsIndex, uint32_t rhsIndex) {
        if (lhsIndex == rhsIndex) {
            return;
        }
        m_entityMap[m_entities[lhsIndex]] = rhsIndex;
        m_entityMap[m_entities[rhsIndex]] = lhsIndex;
        std::swap(m_componentMasks[lhsIndex], m_componentMasks[rhsIndex]);
        std::swap(m_componentIndexCards[lhsIndex], m_componentIndexCards[rhsIndex]);
        std::swap(m_entities[lhsIndex], m_entities[rhsIndex]);
    }
};

template <uint32_t MaxComponents>
uint32_t EntityManager<MaxComponents>::s_componentIndexCounter = 0;

}