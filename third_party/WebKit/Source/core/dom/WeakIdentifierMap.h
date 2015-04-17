// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakIdentifierMap_h
#define WeakIdentifierMap_h

#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

#if !ENABLE(OILPAN)
template<typename T> struct WeakIdentifierMapTraits {
    static void removedFromIdentifierMap(T*) { }
    static void addedToIdentifierMap(T*) { }
};

template<typename T, typename Traits = WeakIdentifierMapTraits<T>> class WeakIdentifierMap {
public:
    ~WeakIdentifierMap()
    {
        ObjectToWeakIdentifierMaps& allMaps = ObjectToWeakIdentifierMaps::instance();
        for (auto& map : m_objectToIdentifier) {
            T* object = map.key;
            if (allMaps.removedFromMap(object, this))
                Traits::removedFromIdentifierMap(object);
        }
    }

    int identifier(T* object)
    {
        int result = m_objectToIdentifier.get(object);
        if (!result) {
            static int s_lastId = 0;
            result = ++s_lastId;
            put(object, result);
        }
        return result;
    }

    T* lookup(int identifier)
    {
        return m_identifierToObject.get(identifier);
    }

    static void notifyObjectDestroyed(T* object)
    {
        ObjectToWeakIdentifierMaps::instance().objectDestroyed(object);
    }

private:
    void put(T* object, int identifier)
    {
        ASSERT(object && !m_objectToIdentifier.contains(object));
        m_objectToIdentifier.set(object, identifier);
        m_identifierToObject.set(identifier, object);

        ObjectToWeakIdentifierMaps& maps = ObjectToWeakIdentifierMaps::instance();
        if (maps.addedToMap(object, this))
            Traits::addedToIdentifierMap(object);
    }

    class ObjectToWeakIdentifierMaps {
        typedef WeakIdentifierMap<T> IdentifierMap;
    public:
        bool addedToMap(T* object, IdentifierMap* map)
        {
            typename ObjectToMapList::AddResult result = m_objectToMapList.add(object, nullptr);
            if (result.isNewEntry)
                result.storedValue->value = adoptPtr(new MapList());
            result.storedValue->value->append(map);
            return result.isNewEntry;
        }

        bool removedFromMap(T* object, IdentifierMap* map)
        {
            typename ObjectToMapList::iterator it = m_objectToMapList.find(object);
            ASSERT(it != m_objectToMapList.end());
            MapList* mapList = it->value.get();
            size_t position = mapList->find(map);
            ASSERT(position != kNotFound);
            mapList->remove(position);
            if (mapList->isEmpty()) {
                m_objectToMapList.remove(it);
                return true;
            }
            return false;
        }

        void objectDestroyed(T* object)
        {
            OwnPtr<MapList> maps = m_objectToMapList.take(object);
            for (auto& map : *maps)
                map->objectDestroyed(object);
        }

        static ObjectToWeakIdentifierMaps& instance()
        {
            DEFINE_STATIC_LOCAL(ObjectToWeakIdentifierMaps, self, ());
            return self;
        }

    private:
        typedef Vector<IdentifierMap*, 1> MapList;
        typedef HashMap<T*, OwnPtr<MapList>> ObjectToMapList;
        ObjectToMapList m_objectToMapList;
    };

    void objectDestroyed(T* object)
    {
        int identifier = m_objectToIdentifier.take(object);
        ASSERT(identifier);
        m_identifierToObject.remove(identifier);
    }

    typedef HashMap<T*, int> ObjectToIdentifier;
    typedef HashMap<int, T*> IdentifierToObject;

    ObjectToIdentifier m_objectToIdentifier;
    IdentifierToObject m_identifierToObject;
};

#else // ENABLE(OILPAN)

template<typename T> class WeakIdentifierMap : public GarbageCollected<WeakIdentifierMap<T>> {
public:
    WeakIdentifierMap()
        : m_objectToIdentifier(new ObjectToIdentifier())
        , m_identifierToObject(new IdentifierToObject())
    {
    }

    int identifier(T* object)
    {
        int result = m_objectToIdentifier->get(object);
        if (!result) {
            static int s_lastId = 0;
            result = ++s_lastId;
            put(object, result);
        }
        return result;
    }

    T* lookup(int identifier)
    {
        return m_identifierToObject->get(identifier);
    }

    static void notifyObjectDestroyed(T* object) { }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_objectToIdentifier);
        visitor->trace(m_identifierToObject);
    }

private:
    void put(T* object, int identifier)
    {
        ASSERT(object && !m_objectToIdentifier->contains(object));
        m_objectToIdentifier->set(object, identifier);
        m_identifierToObject->set(identifier, object);
    }

    typedef HeapHashMap<WeakMember<T>, int> ObjectToIdentifier;
    typedef HeapHashMap<int, WeakMember<T>> IdentifierToObject;

    Member<ObjectToIdentifier> m_objectToIdentifier;
    Member<IdentifierToObject> m_identifierToObject;
};

#endif // ENABLE(OILPAN)

}

#endif // WeakIdentifierMap_h
