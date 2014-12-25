// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AsyncCallChainMap_h
#define AsyncCallChainMap_h

#include "core/inspector/AsyncCallChain.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

template <class K>
class AsyncCallChainMap final {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    using MapType = WillBeHeapHashMap<K, RefPtrWillBeMember<AsyncCallChain>>;
    explicit AsyncCallChainMap(InspectorDebuggerAgent* debuggerAgent)
        : m_debuggerAgent(debuggerAgent)
    {
    }

    ~AsyncCallChainMap()
    {
        // Verify that this object has been explicitly cleared already.
        ASSERT(!m_debuggerAgent);
    }

    void dispose()
    {
        clear();
        m_debuggerAgent = nullptr;
    }

    void clear()
    {
        ASSERT(m_debuggerAgent);
        for (auto it : m_asyncCallChains)
            m_debuggerAgent->didCompleteAsyncOperation(it.value.get());
        m_asyncCallChains.clear();
    }

    void set(typename MapType::KeyPeekInType key, PassRefPtrWillBeRawPtr<AsyncCallChain> chain)
    {
        m_asyncCallChains.set(key, chain);
    }

    bool contains(typename MapType::KeyPeekInType key) const
    {
        return m_asyncCallChains.contains(key);
    }

    PassRefPtrWillBeRawPtr<AsyncCallChain> get(typename MapType::KeyPeekInType key) const
    {
        return m_asyncCallChains.get(key);
    }

    void remove(typename MapType::KeyPeekInType key)
    {
        ASSERT(m_debuggerAgent);
        RefPtrWillBeRawPtr<AsyncCallChain> chain = m_asyncCallChains.take(key);
        if (chain)
            m_debuggerAgent->didCompleteAsyncOperation(chain.get());
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_debuggerAgent);
        visitor->trace(m_asyncCallChains);
    }

private:
    RawPtrWillBeMember<InspectorDebuggerAgent> m_debuggerAgent;
    MapType m_asyncCallChains;
};

} // namespace blink


#endif // AsyncCallChainMap_h
