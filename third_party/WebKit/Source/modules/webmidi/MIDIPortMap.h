// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIPortMap_h
#define MIDIPortMap_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Iterable.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

template <typename T>
class MIDIPortMap : public GarbageCollected<MIDIPortMap<T>>, public PairIterable<String, T*> {
public:
    explicit MIDIPortMap(const HeapHashMap<String, Member<T>>& entries) : m_entries(entries) { }

    // IDL attributes / methods
    size_t size() const { return m_entries.size(); }
    T* get(const String& key) const;
    bool has(const String& key) const { return m_entries.contains(key); }

    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_entries);
    }

private:
    typedef HeapHashMap<String, Member<T>> MapType;
    typedef typename HeapHashMap<String, Member<T>>::const_iterator IteratorType;

    typename PairIterable<String, T*>::IterationSource* startIteration(ScriptState*, ExceptionState&) override
    {
        return new MapIterationSource(this, m_entries.begin(), m_entries.end());
    }

    // Note: This template class relies on the fact that m_map.m_entries will
    // never be modified once it is created.
    class MapIterationSource final : public PairIterable<String, T*>::IterationSource {
    public:
        MapIterationSource(MIDIPortMap<T>* map, IteratorType iterator, IteratorType end)
            : m_map(map)
            , m_iterator(iterator)
            , m_end(end)
        {
        }

        bool next(ScriptState* scriptState, String& key, T*& value, ExceptionState&) override
        {
            if (m_iterator == m_end)
                return false;
            key = m_iterator->key;
            value = m_iterator->value;
            ++m_iterator;
            return true;
        }

        void trace(Visitor* visitor) override
        {
            visitor->trace(m_map);
            PairIterable<String, T*>::IterationSource::trace(visitor);
        }

    private:
        // m_map is stored just for keeping it alive. It needs to be kept
        // alive while JavaScript holds the iterator to it.
        const Member<const MIDIPortMap<T>> m_map;
        IteratorType m_iterator;
        const IteratorType m_end;
    };

    const MapType m_entries;
};

template <typename T>
T* MIDIPortMap<T>::get(const String& key) const
{
    return has(key) ? m_entries.get(key) : 0;
}

} // namespace blink

#endif
