// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMNodeIds_h
#define DOMNodeIds_h

#include "core/CoreExport.h"
#include "core/dom/Node.h"
#include "core/dom/WeakIdentifierMap.h"

namespace blink {

#if !ENABLE(OILPAN)
template<> struct WeakIdentifierMapTraits<Node> {
    static void removedFromIdentifierMap(Node*);
    static void addedToIdentifierMap(Node*);
};
#endif

extern template class WeakIdentifierMap<Node>;
using WeakNodeMap = WeakIdentifierMap<Node>;

class CORE_EXPORT DOMNodeIds {
public:
    static int idForNode(Node*);
    static Node* nodeForId(int);
};

} // namespace blink


#endif // DOMNodeIds_h
