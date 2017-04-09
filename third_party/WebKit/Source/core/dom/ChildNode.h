// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChildNode_h
#define ChildNode_h

#include "core/dom/Node.h"

namespace blink {

class ChildNode {
 public:
  static void before(Node& node,
                     const HeapVector<NodeOrString>& nodes,
                     ExceptionState& exception_state) {
    return node.Before(nodes, exception_state);
  }

  static void after(Node& node,
                    const HeapVector<NodeOrString>& nodes,
                    ExceptionState& exception_state) {
    return node.After(nodes, exception_state);
  }

  static void replaceWith(Node& node,
                          const HeapVector<NodeOrString>& nodes,
                          ExceptionState& exception_state) {
    return node.ReplaceWith(nodes, exception_state);
  }

  static void remove(Node& node, ExceptionState& exception_state) {
    return node.remove(exception_state);
  }
};

}  // namespace blink

#endif  // ChildNode_h
