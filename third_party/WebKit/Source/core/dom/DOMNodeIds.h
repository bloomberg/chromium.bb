// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMNodeIds_h
#define DOMNodeIds_h

#include "core/CoreExport.h"
#include "core/dom/Node.h"
#include "core/dom/WeakIdentifierMap.h"
#include "platform/wtf/Allocator.h"

namespace blink {

using DOMNodeId = uint64_t;

DECLARE_WEAK_IDENTIFIER_MAP(Node, DOMNodeId);

static const DOMNodeId kInvalidDOMNodeId = 0;

class CORE_EXPORT DOMNodeIds {
  STATIC_ONLY(DOMNodeIds);

 public:
  static DOMNodeId IdForNode(Node*);
  static Node* NodeForId(DOMNodeId);
};

}  // namespace blink

#endif  // DOMNodeIds_h
