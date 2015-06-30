// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorIdentifiers_h
#define InspectorIdentifiers_h

#include "core/dom/WeakIdentifierMap.h"
#include "core/inspector/IdentifiersFactory.h"

namespace blink {

struct InspectorIdentifierGenerator {
    using IdentifierType = String;
    static IdentifierType next()
    {
        return IdentifiersFactory::createIdentifier();
    }
};

template<typename T> class InspectorIdentifiers {
public:
    static String identifier(T* object) { return map().identifier(object); }
    static T* lookup(const String& identifier) { return map().lookup(identifier); }
    static void notifyObjectDestroyed(T* object) { map().notifyObjectDestroyed(object); }

private:
    using IdentifierMap = WeakIdentifierMap<T, InspectorIdentifierGenerator>;
    InspectorIdentifiers() { }

    static IdentifierMap& map();
};

#define DECLARE_INSPECTOR_IDENTIFIERS(T) \
    DECLARE_WEAK_IDENTIFIER_MAP(T, InspectorIdentifierGenerator); \
    template<> InspectorIdentifiers<T>::IdentifierMap& InspectorIdentifiers<T>::map(); \
    extern template class InspectorIdentifiers<T>;

#define DEFINE_INSPECTOR_IDENTIFIERS(T) \
    template class InspectorIdentifiers<T>; \
    DEFINE_WEAK_IDENTIFIER_MAP(T, InspectorIdentifierGenerator); \
    template<> InspectorIdentifiers<T>::IdentifierMap& InspectorIdentifiers<T>::map() \
    { \
        DEFINE_STATIC_LOCAL(IdentifierMap::ReferenceType, identifierMap, (new IdentifierMap)); \
        return *identifierMap; \
    }

} // namespace blink

#endif // #ifndef InspectorIdentifiers_h
