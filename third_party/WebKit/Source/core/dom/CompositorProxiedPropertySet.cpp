// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxiedPropertySet.h"

#include "wtf/PassOwnPtr.h"

namespace blink {

PassOwnPtr<CompositorProxiedPropertySet> CompositorProxiedPropertySet::create()
{
    return adoptPtr(new CompositorProxiedPropertySet);
}

CompositorProxiedPropertySet::CompositorProxiedPropertySet()
{
    memset(m_counts, 0, sizeof(m_counts));
}

CompositorProxiedPropertySet::~CompositorProxiedPropertySet() {}

bool CompositorProxiedPropertySet::isEmpty() const
{
    return !proxiedProperties();
}

void CompositorProxiedPropertySet::increment(uint32_t mutableProperties)
{
    for (int i = 0; i < kNumWebCompositorMutableProperties; ++i) {
        if (mutableProperties & (1 << i))
            ++m_counts[i];
    }
}

void CompositorProxiedPropertySet::decrement(uint32_t mutableProperties)
{
    for (int i = 0; i < kNumWebCompositorMutableProperties; ++i) {
        if (mutableProperties & (1 << i)) {
            ASSERT(m_counts[i]);
            --m_counts[i];
        }
    }
}

uint32_t CompositorProxiedPropertySet::proxiedProperties() const
{
    uint32_t properties = WebCompositorMutablePropertyNone;
    for (int i = 0; i < kNumWebCompositorMutableProperties; ++i) {
        if (m_counts[i])
            properties |= 1 << i;
    }
    return properties;
}

} // namespace blink
