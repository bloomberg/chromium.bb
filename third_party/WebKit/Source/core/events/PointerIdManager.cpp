// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/events/PointerIdManager.h"

namespace blink {

namespace {

inline int toInt(WebPointerProperties::PointerType t) { return static_cast<int>(t); }

} // namespace

PointerIdManager::PointerIdManager()
{
    clear();
}

PointerIdManager::~PointerIdManager()
{
    clear();
}

void PointerIdManager::clear()
{
    for (int type = 0; type <= toInt(WebPointerProperties::PointerType::LastEntry); type++) {
        m_ids[type].clear();
        m_hasPrimaryId[type] = false;
    }
}

void PointerIdManager::add(WebPointerProperties::PointerType type, unsigned id)
{
    if (m_ids[toInt(type)].isEmpty())
        m_hasPrimaryId[toInt(type)] = true;
    m_ids[toInt(type)].add(id);
}

void PointerIdManager::remove(WebPointerProperties::PointerType type, unsigned id)
{
    if (isPrimary(type, id)) {
        m_ids[toInt(type)].removeFirst();
        m_hasPrimaryId[toInt(type)] = false;
    } else {
        // Note that simply counting the number of ids instead of storing all of them is not enough.
        // When id is absent, remove() should be a no-op.
        m_ids[toInt(type)].remove(id);
    }
}

bool PointerIdManager::isPrimary(WebPointerProperties::PointerType type, unsigned id)
{
    return m_hasPrimaryId[toInt(type)] && m_ids[toInt(type)].first() == id;
}

} // namespace blink
