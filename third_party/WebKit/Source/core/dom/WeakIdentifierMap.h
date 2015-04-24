// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakIdentifierMap_h
#define WeakIdentifierMap_h

#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

template<typename T> struct IdentifierGenerator;

template<> struct IdentifierGenerator<int> {
    using IdentifierType = int;
    static IdentifierType next()
    {
        static int s_lastId = 0;
        return ++s_lastId;
    }
};

template<typename T> struct WeakIdentifierMapTraits {
    static void removedFromIdentifierMap(T*) { }
    static void addedToIdentifierMap(T*) { }
};

template<typename T,
    typename Generator = IdentifierGenerator<int>,
    typename Traits = WeakIdentifierMapTraits<T>,
    bool isGarbageCollected = IsGarbageCollectedType<T>::value> class WeakIdentifierMap;

template<typename T, typename Generator, typename Traits> class WeakIdentifierMap<T, Generator, Traits, false> {
public:
    using IdentifierType = typename Generator::IdentifierType;
    using ReferenceType = RawPtr<WeakIdentifierMap<T, Generator, Traits, false>>;

    ~WeakIdentifierMap()
    {
        ObjectToWeakIdentifierMaps& allMaps = ObjectToWeakIdentifierMaps::instance();
        for (auto& map : m_objectToIdentifier) {
            T* object = map.key;
            if (allMaps.removedFromMap(object, this))
                Traits::removedFromIdentifierMap(object);
        }
    }

    IdentifierType identifier(T* object)
    {
        IdentifierType result = m_objectToIdentifier.get(object);

        if (WTF::isHashTraitsEmptyValue<HashTraits<IdentifierType>>(result)) {
            result = Generator::next();
            put(object, result);
        }
        return result;
    }

    T* lookup(IdentifierType identifier)
    {
        return m_identifierToObject.get(identifier);
    }

    static void notifyObjectDestroyed(T* object)
    {
        ObjectToWeakIdentifierMaps::instance().objectDestroyed(object);
    }

private:
    void put(T* object, IdentifierType identifier)
    {
        ASSERT(object && !m_objectToIdentifier.contains(object));
        m_objectToIdentifier.set(object, identifier);
        m_identifierToObject.set(identifier, object);

        ObjectToWeakIdentifierMaps& maps = ObjectToWeakIdentifierMaps::instance();
        if (maps.addedToMap(object, this))
            Traits::addedToIdentifierMap(object);
    }

    class ObjectToWeakIdentifierMaps {
        using IdentifierMap = WeakIdentifierMap<T, Generator, Traits>;
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
            if (OwnPtr<MapList> maps = m_objectToMapList.take(object)) {
                for (auto& map : *maps)
                    map->objectDestroyed(object);
            }
        }

        static ObjectToWeakIdentifierMaps& instance()
        {
            DEFINE_STATIC_LOCAL(ObjectToWeakIdentifierMaps, self, ());
            return self;
        }

    private:
        using MapList = Vector<IdentifierMap*, 1>;
        using ObjectToMapList = HashMap<T*, OwnPtr<MapList>>;
        ObjectToMapList m_objectToMapList;
    };

    void objectDestroyed(T* object)
    {
        int identifier = m_objectToIdentifier.take(object);
        ASSERT(identifier);
        m_identifierToObject.remove(identifier);
    }

    using ObjectToIdentifier = HashMap<T*, IdentifierType>;
    using IdentifierToObject = HashMap<IdentifierType, T*>;

    ObjectToIdentifier m_objectToIdentifier;
    IdentifierToObject m_identifierToObject;
};

template<typename T, typename Generator, typename Traits> class WeakIdentifierMap<T, Generator, Traits, true>
    : public GarbageCollected<WeakIdentifierMap<T, Generator, Traits, true>> {
public:
    using IdentifierType = typename Generator::IdentifierType;
    using ReferenceType = Persistent<WeakIdentifierMap<T, Generator, Traits, true>>;

    WeakIdentifierMap()
        : m_objectToIdentifier(new ObjectToIdentifier())
        , m_identifierToObject(new IdentifierToObject())
    {
    }

    IdentifierType identifier(T* object)
    {
        IdentifierType result = m_objectToIdentifier->get(object);

        if (WTF::isHashTraitsEmptyValue<HashTraits<IdentifierType>>(result)) {
            result = Generator::next();
            put(object, result);
        }
        return result;
    }

    T* lookup(IdentifierType identifier)
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
    void put(T* object, IdentifierType identifier)
    {
        ASSERT(object && !m_objectToIdentifier->contains(object));
        m_objectToIdentifier->set(object, identifier);
        m_identifierToObject->set(identifier, object);
    }

    using ObjectToIdentifier = HeapHashMap<WeakMember<T>, IdentifierType>;
    using IdentifierToObject = HeapHashMap<IdentifierType, WeakMember<T>>;

    Member<ObjectToIdentifier> m_objectToIdentifier;
    Member<IdentifierToObject> m_identifierToObject;
};

}

#endif // WeakIdentifierMap_h
