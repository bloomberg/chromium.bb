// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FullyClippedStateStack_h
#define FullyClippedStateStack_h

#include "core/editing/iterators/BitStack.h"
#include "core/editing/iterators/TextIteratorStrategy.h"

namespace blink {

class NodeIterationStrategy;
class NodeTraversal;

template<typename Strategy>
class FullyClippedStateStackAlgorithm : public BitStack {
public:
    FullyClippedStateStackAlgorithm();
    ~FullyClippedStateStackAlgorithm();

    void pushFullyClippedState(Node*);
    void setUpFullyClippedStack(Node*);
};

extern template class FullyClippedStateStackAlgorithm<TextIteratorStrategy>;

using FullyClippedStateStack = FullyClippedStateStackAlgorithm<TextIteratorStrategy>;

} // namespace blink

#endif // FullyClippedStateStack_h
