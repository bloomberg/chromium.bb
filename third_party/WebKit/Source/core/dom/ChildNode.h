// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChildNode_h
#define ChildNode_h

#include "core/dom/Node.h"

namespace blink {

class ChildNode {
public:
    static void remove(Node& node, ExceptionState& exceptionState)
    {
        return node.remove(exceptionState);
    }
};

} // namespace blink

#endif // ChildNode_h
