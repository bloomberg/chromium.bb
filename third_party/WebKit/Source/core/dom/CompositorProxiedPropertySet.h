// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxiedPropertySet_h
#define CompositorProxiedPropertySet_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCompositorMutableProperties.h"

namespace blink {

// Keeps track of the number of proxies bound to each property.
class CompositorProxiedPropertySet final : public NoBaseWillBeGarbageCollectedFinalized<CompositorProxiedPropertySet> {
    WTF_MAKE_NONCOPYABLE(CompositorProxiedPropertySet);
    USING_FAST_MALLOC_WILL_BE_REMOVED(CompositorProxiedPropertySet);
public:
    static PassOwnPtrWillBeRawPtr<CompositorProxiedPropertySet> create();
    virtual ~CompositorProxiedPropertySet();

    DEFINE_INLINE_TRACE() { }

    bool isEmpty() const;
    void increment(uint32_t mutableProperties);
    void decrement(uint32_t mutableProperties);
    uint32_t proxiedProperties() const;

private:
    CompositorProxiedPropertySet();

    unsigned short m_counts[kNumWebCompositorMutableProperties];
};

} // namespace blink

#endif // CompositorProxiedPropertySet_h
