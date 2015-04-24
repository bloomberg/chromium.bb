// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMNodeIds.h"
#include "platform/heap/Handle.h"

namespace blink {

template class WeakIdentifierMap<Node>;

#if !ENABLE(OILPAN)
void WeakIdentifierMapTraits<Node>::removedFromIdentifierMap(Node* node)
{
    node->clearFlag(Node::HasWeakReferencesFlag);
}

void WeakIdentifierMapTraits<Node>::addedToIdentifierMap(Node* node)
{
    node->setFlag(Node::HasWeakReferencesFlag);
}
#endif

static WeakNodeMap& nodeIds()
{
    DEFINE_STATIC_LOCAL(WeakNodeMap::ReferenceType, self, (new WeakNodeMap()));
    return *self;
}

int DOMNodeIds::idForNode(Node* node)
{
    return nodeIds().identifier(node);
}

Node* DOMNodeIds::nodeForId(int id)
{
    return nodeIds().lookup(id);
}

}
