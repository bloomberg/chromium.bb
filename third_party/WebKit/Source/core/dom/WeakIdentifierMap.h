// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakIdentifierMap_h
#define WeakIdentifierMap_h

#include "wtf/HashMap.h"

namespace blink {
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

    void put(T* object, int identifier)
    {
        ASSERT(object && !m_objectToIdentifier.contains(object));
        m_objectToIdentifier.set(object, identifier);
        m_identifierToObject.set(identifier, object);

        ObjectToWeakIdentifierMaps& maps = ObjectToWeakIdentifierMaps::instance();
        if (maps.addedToMap(object, this))
            Traits::addedToIdentifierMap(object);
    }

    int identifier(T* object)
    {
        return m_objectToIdentifier.get(object);
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
    ObjectToIdentifier m_objectToIdentifier;
    typedef HashMap<int, T*> IdentifierToObject;
    IdentifierToObject m_identifierToObject;
};

}
#endif // WeakIdentifierMap_h
