// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMNodeIds.h"

#include "platform/heap/Handle.h"

namespace blink {

DEFINE_WEAK_IDENTIFIER_MAP(Node);

// static
int DOMNodeIds::IdForNode(Node* node) {
  return WeakIdentifierMap<Node>::Identifier(node);
}

// static
Node* DOMNodeIds::NodeForId(int id) {
  return WeakIdentifierMap<Node>::Lookup(id);
}

}  // namespace blink
