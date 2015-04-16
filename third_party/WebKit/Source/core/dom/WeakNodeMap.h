// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakNodeMap_h
#define WeakNodeMap_h

// Oilpan supports weak maps, so we no longer need WeakNodeMap.
#if !ENABLE(OILPAN)

#include "core/dom/Node.h"
#include "core/dom/WeakIdentifierMap.h"

namespace blink {

template<> struct WeakIdentifierMapTraits<Node> {
    static void removedFromIdentifierMap(Node* node) { node->clearFlag(Node::HasWeakReferencesFlag); }
    static void addedToIdentifierMap(Node* node) { node->setFlag(Node::HasWeakReferencesFlag); }
};

typedef WeakIdentifierMap<Node> WeakNodeMap;

} // namespace blink

#endif // !ENABLE(OILPAN)

#endif // WeakNodeMap_h
