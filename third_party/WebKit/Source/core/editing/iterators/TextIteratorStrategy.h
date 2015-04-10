// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextIteratorStrategy_h
#define TextIteratorStrategy_h

#include "core/editing/EditingStrategy.h"

namespace blink {

class Position;

class TextIteratorStrategy : public EditingStrategy {
public:
    static ContainerNode* parentOrShadowHostNode(const Node&);
    static int shadowDepthOf(const Node&, const Node&);
};

} // namespace blink

#endif // TextIteratorStrategy_h
