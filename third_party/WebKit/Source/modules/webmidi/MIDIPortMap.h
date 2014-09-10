// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIPortMap_h
#define MIDIPortMap_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Iterator.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

template <typename T>
class MIDIPortMap : public GarbageCollected<MIDIPortMap<T> > {
public:
    explicit MIDIPortMap(const HeapHashMap<String, Member<T> >& entries) : m_entries(entries) { }

    // IDL attributes / methods
    size_t size() const { return m_entries.size(); }
    Iterator* keys();
    Iterator* entries();
    Iterator* values();
    T* get(const String& key) const;
    bool has(const String& key) const { return m_entries.contains(key); }
    Iterator* iterator(ScriptState*, ExceptionState&) { return entries(); }

    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_entries);
    }

private:
    typedef HeapHashMap<String, Member<T> > MapType;
    typedef typename HeapHashMap<String, Member<T> >::const_iterator IteratorType;
    struct KeySelector {
        static const String& select(ScriptState*, IteratorType i) { return i->key; }
    };
    struct ValueSelector {
        static T* select(ScriptState*, IteratorType i) { return i->value; }
    };
    struct EntrySelector {
        static Vector<ScriptValue> select(ScriptState* scriptState, IteratorType i)
        {
            Vector<ScriptValue> entry;
            entry.append(ScriptValue(scriptState, v8String(scriptState->isolate(), i->key)));
            entry.append(ScriptValue(scriptState, V8ValueTraits<T*>::toV8Value(i->value, scriptState->context()->Global(), scriptState->isolate())));
            return entry;
        }
    };

    // Note: This template class relies on the fact that m_map.m_entries will
    // never be modified once it is created.
    template <typename Selector>
    class MapIterator : public Iterator {
    public:
        MapIterator(MIDIPortMap<T>* map, IteratorType iterator, IteratorType end)
            : m_map(map)
            , m_iterator(iterator)
            , m_end(end)
        {
        }

        virtual ScriptValue next(ScriptState* scriptState, ExceptionState&) OVERRIDE
        {
            if (m_iterator == m_end)
                return ScriptValue(scriptState, v8DoneIteratorResult(scriptState->isolate()));
            ScriptValue result(scriptState, v8IteratorResult(scriptState, Selector::select(scriptState, m_iterator)));
            ++m_iterator;
            return result;
        }

        virtual ScriptValue next(ScriptState* scriptState, ScriptValue, ExceptionState& exceptionState) OVERRIDE
        {
            return next(scriptState, exceptionState);
        }

        virtual void trace(Visitor* visitor) OVERRIDE
        {
            visitor->trace(m_map);
            Iterator::trace(visitor);
        }

    private:
        // m_map is stored just for keeping it alive. It needs to be kept
        // alive while JavaScript holds the iterator to it.
        const Member<const MIDIPortMap<T> > m_map;
        IteratorType m_iterator;
        const IteratorType m_end;
    };

    const MapType m_entries;
};

template <typename T>
Iterator* MIDIPortMap<T>::keys()
{
    return new MapIterator<KeySelector>(this, m_entries.begin(), m_entries.end());
}

template <typename T>
Iterator* MIDIPortMap<T>::entries()
{
    return new MapIterator<EntrySelector>(this, m_entries.begin(), m_entries.end());
}

template <typename T>
Iterator* MIDIPortMap<T>::values()
{
    return new MapIterator<ValueSelector>(this, m_entries.begin(), m_entries.end());
}

template <typename T>
T* MIDIPortMap<T>::get(const String& key) const
{
    return has(key) ? m_entries.get(key) : 0;
}

} // namespace blink

#endif
