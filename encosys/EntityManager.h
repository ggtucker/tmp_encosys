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
#include "System.h"
#include "FunctionTraits.h"

namespace ECS {

template <typename TComponentList>
class EntityManager {
private:
    using ComponentMask = std::bitset<TComponentList::Size>;
    using ComponentIndexCard = std::array<uint32_t, TComponentList::Size>;
    using ComponentPools = typename TComponentList::template WrapTypes<BlockObjectPool>::ListTuple;
    using SystemCallback = std::function<void(float)>;

public:
    EntityManager () = default;
    virtual ~EntityManager ();

    Entity Create (bool active = true);
    void Destroy (const Entity& id);
    bool IsActive (const Entity& id) const;
    void SetActive (const Entity& id, bool active);
    bool IsValid (const Entity& id) const;

    template <typename TComponent, typename... TArgs>
    void AddComponent(const Entity& id, TArgs&&... args);

    template <typename TComponent>
    TComponent& GetComponent (const Entity& id);

    template <typename TComponent>
    void RemoveComponent (const Entity& id);

    template <typename TFunc>
    void ForEach (TFunc&& callback);

    template <typename TObject, typename TFunc>
    void ForEach (TObject* self, TFunc&& callback);

    template <typename TSystem, typename... TArgs>
    void RegisterSystem (TArgs&&... args);

    void RunSystems (float deltaTime);

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
    ComponentPools m_componentPools{};

    // m_systems contains the list of registered system callbacks.
    std::vector<SystemCallback> m_systems{};

    // m_entityActiveCount is the number of entities that are currently active.
    // Active entities are packed together at the beginning of the m_componentMasks array.
    uint32_t m_entityActiveCount{0};

private:

    template <typename TComponent>
    static constexpr std::size_t GetComponentIndex ();

    // Component pools
    template <typename TComponent>
    BlockObjectPool<std::decay_t<TComponent>>& GetComponentPool ();

    // ForEach helper
    template <typename TFunc, typename... Args, std::size_t... Seq>
    void UnpackAndCallback (uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>);

    // ForEachMember helper
    template <typename TObject, typename TFunc, typename... Args, std::size_t... Seq>
    void UnpackAndCallbackMember (TObject* self, uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>);

    // Index helpers
    bool IndexIsActive (uint32_t index) const;
    void IndexSetActive (uint32_t& index, bool active);

    // Other helpers
    void SwapEntities (uint32_t lhsIndex, uint32_t rhsIndex);
};

template <typename TComponentList>
EntityManager<TComponentList>::~EntityManager() {
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        Destroy(m_entities[i]);
    }
}

template <typename TComponentList>
Entity EntityManager<TComponentList>::Create (bool active) {
    Entity id{ m_entityIdCounter };
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

template <typename TComponentList>
void EntityManager<TComponentList>::Destroy (const Entity& id) {
    auto entityIter = m_entityMap.find(id);
    ECS_ASSERT_(entityIter != m_entityMap.cend());

    auto index = entityIter->second;
    auto& mask = m_componentMasks[index];
    auto& indexCard = m_componentIndexCards[index];
    TComponentList::ForTypes([this, &mask, &indexCard](auto t) {
        (void)t;
        if (mask.test(TComponentList::IndexOf<TYPE_OF(t)>())) {
            std::get<TComponentList::IndexOf<TYPE_OF(t)>()>(m_componentPools).Destroy(indexCard[TComponentList::IndexOf<TYPE_OF(t)>()]);
        }
    });
    IndexSetActive(index, false);
    // Swap this entity to the end so we can destroy it.
    if (index < m_entities.size() - 1) {
        SwapEntities(index, m_entities.size() - 1);
    }
    m_entityMap.erase(entityIter);
    m_componentMasks.pop_back();
    m_componentIndexCards.pop_back();
    m_entities.pop_back();
}

template <typename TComponentList>
bool EntityManager<TComponentList>::IsActive (const Entity& id) const {
    auto it = m_entityMap.find(id);
    return (it != m_entityMap.cend()) ? IndexIsActive(it->second) : false;
}

template <typename TComponentList>
void EntityManager<TComponentList>::SetActive (const Entity& id, bool active) {
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

template <typename TComponentList>
bool EntityManager<TComponentList>::IsValid (const Entity& id) const {
    return id != c_invalidEntity && m_entityMap.find(id) != m_entityMap.cend();
}

template <typename TComponentList>
template <typename TComponent, typename... TArgs>
void EntityManager<TComponentList>::AddComponent (const Entity& id, TArgs&&... args) {
    static_assert(TComponentList::Contains<TComponent>(),
        "Cannot add component of a type that has not been registered in the ECS::EntityManager");
    ECS_ASSERT_(IsValid(id));
    auto index = m_entityMap[id];
    auto& mask = m_componentMasks[index];
    auto& indexCard = m_componentIndexCards[index];
    ECS_ASSERT_(mask.test(GetComponentIndex<TComponent>()) == false);
    auto& pool = GetComponentPool<TComponent>();
    indexCard[GetComponentIndex<TComponent>()] = pool.Create(std::forward<TArgs>(args)...);
    mask.set(GetComponentIndex<TComponent>());
}

template <typename TComponentList>
template <typename TComponent>
TComponent& EntityManager<TComponentList>::GetComponent (const Entity& id) {
    static_assert(TComponentList::Contains<TComponent>(),
        "Cannot get component of a type that has not been registered in the ECS::EntityManager");
    ECS_ASSERT_(IsValid(id));
    auto index = m_entityMap[id];
    auto& mask = m_componentMasks[index];
    auto& indexCard = m_componentIndexCards[index];
    ECS_ASSERT_(mask.test(GetComponentIndex<TComponent>()) == true);
    auto& pool = GetComponentPool<TComponent>();
    return pool.GetObject(indexCard[GetComponentIndex<TComponent>()]);
}

template <typename TComponentList>
template <typename TComponent>
void EntityManager<TComponentList>::RemoveComponent (const Entity& id) {
    static_assert(TComponentList::Contains<TComponent>(),
        "Cannot remove component of a type that has not been registered in the ECS::EntityManager");
    ECS_ASSERT_(IsValid(id));
    auto index = m_entityMap[id];
    auto& mask = m_componentMasks[index];
    auto& indexCard = m_componentIndexCards[index];
    ECS_ASSERT_(mask.test(GetComponentIndex<TComponent>()) == true);
    auto& pool = GetComponentPool<TComponent>();
    pool.Destroy(indexCard[GetComponentIndex<TComponent>()]);
    mask.set(GetComponentIndex<TComponent>(), false);
}

template <typename TComponentList>
template <typename TFunc>
void EntityManager<TComponentList>::ForEach (TFunc&& callback) {
    using FTraits = FunctionTraits<decltype(callback)>;
    static_assert(FTraits::ArgCount > 0, "First callback param must be ECS::Entity.");
    static_assert(std::is_same<FTraits::Arg<0>, Entity>::value, "First callback param must be ECS::Entity.");
    using FComponentArgs = typename FTraits::Args::RemoveFirst;

    ComponentMask targetMask{};
    FComponentArgs::ForTypes([&targetMask] (auto t) {
        (void)t;
        static_assert(TComponentList::Contains<std::decay_t<TYPE_OF(t)>>(),
            "Cannot get component of a type that has not been registered in the ECS::EntityManager");
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

template <typename TComponentList>
template <typename TObject, typename TFunc>
void EntityManager<TComponentList>::ForEach (TObject* self, TFunc&& callback) {
    using FTraits = FunctionTraits<decltype(callback)>;
    static_assert(FTraits::ArgCount > 0, "First callback param must be ECS::Entity.");
    static_assert(std::is_same<FTraits::Arg<0>, Entity>::value, "First callback param must be ECS::Entity.");
    using FComponentArgs = typename FTraits::Args::RemoveFirst;

    ComponentMask targetMask{};
    FComponentArgs::ForTypes([&targetMask] (auto t) {
        (void)t;
        static_assert(TComponentList::Contains<std::decay_t<TYPE_OF(t)>>(),
            "Cannot get component of a type that has not been registered in the ECS::EntityManager");
        targetMask.set(GetComponentIndex<TYPE_OF(t)>());
    });

    for (uint32_t i = 0; i < m_entityActiveCount; ++i) {
        if ((targetMask & m_componentMasks[i]) == targetMask) {
            UnpackAndCallbackMember(
                self,
                i,
                callback,
                FComponentArgs{},
                typename GenerateSequence<FTraits::ArgCount>::Type{}
            );
        }
    }
}

template <typename TComponentList>
template <typename TSystem, typename... TArgs>
void EntityManager<TComponentList>::RegisterSystem (TArgs&&... args) {
    static_assert(std::is_base_of<System, TSystem>::value, "TSystem must be derived from System<EntityManager<TComponentList>>.");
    m_systems.push_back([this, system = TSystem(std::forward<TArgs>(args)...)](float deltaTime) mutable {
        system.m_entityManager = this;
        system.m_deltaTime = deltaTime;
        system.PreUpdate();
        ForEach(&system, &TSystem::Update);
        system.PostUpdate();
    });
}

template <typename TComponentList>
void EntityManager<TComponentList>::RunSystems (float deltaTime) {
    for (const auto& system : m_systems) {
        system(deltaTime);
    }
}

template <typename TComponentList>
template <typename TComponent>
static constexpr std::size_t EntityManager<TComponentList>::GetComponentIndex () {
    return TComponentList::IndexOf<std::decay_t<TComponent>>();
}

template <typename TComponentList>
template <typename TComponent>
BlockObjectPool<std::decay_t<TComponent>>& EntityManager<TComponentList>::GetComponentPool () {
    return std::get<TComponentList::IndexOf<std::decay_t<TComponent>>()>(m_componentPools);
}

template <typename TComponentList>
template <typename TFunc, typename... Args, std::size_t... Seq>
void EntityManager<TComponentList>::UnpackAndCallback (uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>) {
    auto& indexCard = m_componentIndexCards[entityIndex];
    auto params = std::make_tuple(
        m_entities[entityIndex],
        std::ref(GetComponentPool<Args>().GetObject(indexCard[GetComponentIndex<Args>()]))...
    );
    callback(std::get<Seq>(params)...);
}

template <typename TComponentList>
template <typename TObject, typename TFunc, typename... Args, std::size_t... Seq>
void EntityManager<TComponentList>::UnpackAndCallbackMember (TObject* self, uint32_t entityIndex, TFunc&& callback, TypeList<Args...>, Sequence<Seq...>) {
    auto& indexCard = m_componentIndexCards[entityIndex];
    auto params = std::make_tuple(
        m_entities[entityIndex],
        std::ref(GetComponentPool<Args>().GetObject(indexCard[GetComponentIndex<Args>()]))...
    );
    (self->*callback)(std::get<Seq>(params)...);
}

template <typename TComponentList>
bool EntityManager<TComponentList>::IndexIsActive (uint32_t index) const {
    return index < m_entityActiveCount;
}

template <typename TComponentList>
void EntityManager<TComponentList>::IndexSetActive (uint32_t& index, bool active) {
    if (active == IndexIsActive(index)) {
        return;
    }
    const uint32_t newIndex = (m_entityActiveCount < m_entities.size()) ? m_entityActiveCount : (m_entities.size() - 1);
    SwapEntities(index, newIndex);
    m_entityActiveCount += (active ? 1 : -1);
    index = newIndex;
}

template <typename TComponentList>
void EntityManager<TComponentList>::SwapEntities (uint32_t lhsIndex, uint32_t rhsIndex) {
    if (lhsIndex == rhsIndex) {
        return;
    }
    m_entityMap[m_entities[lhsIndex]] = rhsIndex;
    m_entityMap[m_entities[rhsIndex]] = lhsIndex;
    std::swap(m_componentMasks[lhsIndex], m_componentMasks[rhsIndex]);
    std::swap(m_componentIndexCards[lhsIndex], m_componentIndexCards[rhsIndex]);
    std::swap(m_entities[lhsIndex], m_entities[rhsIndex]);
}

}