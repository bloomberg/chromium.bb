/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "RefCountedLeakCounter.h"
#include "wtf/Assertions.h"

#if ENABLE(ASSERT)
#include "wtf/Atomics.h"
#endif

namespace WTF {

#if !ENABLE(ASSERT)

RefCountedLeakCounter::RefCountedLeakCounter(const char*) { }
RefCountedLeakCounter::~RefCountedLeakCounter() { }

void RefCountedLeakCounter::increment() { }
void RefCountedLeakCounter::decrement() { }

#else

#define LOG_CHANNEL_PREFIX Log
static WTFLogChannel LogRefCountedLeaks = { WTFLogChannelOn };

RefCountedLeakCounter::RefCountedLeakCounter(const char* description)
    : m_description(description)
{
}

RefCountedLeakCounter::~RefCountedLeakCounter()
{
    if (!m_count)
        return;

    WTF_LOG(RefCountedLeaks, "LEAK: %u %s", m_count, m_description);
}

void RefCountedLeakCounter::increment()
{
    atomicIncrement(&m_count);
}

void RefCountedLeakCounter::decrement()
{
    atomicDecrement(&m_count);
}

#endif

} // namespace WTF
