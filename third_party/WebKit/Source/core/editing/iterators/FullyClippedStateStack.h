// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FullyClippedStateStack_h
#define FullyClippedStateStack_h

#include "core/editing/iterators/BitStack.h"

namespace blink {

class FullyClippedStateStack : public BitStack {
public:
    FullyClippedStateStack();
    ~FullyClippedStateStack();

    void pushFullyClippedState(Node*);
    void setUpFullyClippedStack(Node*);
};

} // namespace blink

#endif // FullyClippedStateStack_h
