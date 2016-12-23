#pragma once

#include "EntityManager.h"

using namespace ECS;

uint32_t EntityManager::s_componentIndexCounter = 0;

EntityManager::~EntityManager() {
    for (uint32_t i = 0; i < m_entities.size(); ++i) {
        Destroy(m_entities[i]);
    }
}

Entity EntityManager::Create (bool active) {
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

void EntityManager::Destroy (const Entity& id) {
    auto entityIter = m_entityMap.find(id);
    ECS_ASSERT_(entityIter != m_entityMap.cend());

    auto index = m_entityMap[id];
    auto& mask = m_componentMasks[index];
    auto& indexCard = m_componentIndexCards[index];
    for (uint32_t i = 0; i < mask.size(); ++i) {
        if (mask.test(i)) {
            m_componentPools[i]->Destroy(indexCard[i]);
        }
    }
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

bool EntityManager::IsActive (const Entity& id) const {
    auto it = m_entityMap.find(id);
    return (it != m_entityMap.cend()) ? IndexIsActive(it->second) : false;
}

void EntityManager::SetActive (const Entity& id, bool active) {
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

bool EntityManager::IsValid (const Entity& id) const {
    return id != c_invalidEntity && m_entityMap.find(id) != m_entityMap.cend();
}

bool EntityManager::IndexIsActive (uint32_t index) const {
    return index < m_entityActiveCount;
}

void EntityManager::IndexSetActive (uint32_t index, bool active) {
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

void EntityManager::SwapEntities (uint32_t lhsIndex, uint32_t rhsIndex) {
    if (lhsIndex == rhsIndex) {
        return;
    }
    m_entityMap[m_entities[lhsIndex]] = rhsIndex;
    m_entityMap[m_entities[rhsIndex]] = lhsIndex;
    std::swap(m_componentMasks[lhsIndex], m_componentMasks[rhsIndex]);
    std::swap(m_componentIndexCards[lhsIndex], m_componentIndexCards[rhsIndex]);
    std::swap(m_entities[lhsIndex], m_entities[rhsIndex]);
}
