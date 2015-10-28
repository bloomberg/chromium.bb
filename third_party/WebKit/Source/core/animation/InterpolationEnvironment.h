// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEnvironment_h
#define InterpolationEnvironment_h

#include "wtf/Allocator.h"

namespace blink {

class StyleResolverState;

class InterpolationEnvironment {
    STACK_ALLOCATED();
public:
    explicit InterpolationEnvironment(StyleResolverState& state)
        : m_state(&state)
    { }

    StyleResolverState& state() { ASSERT(m_state); return *m_state; }
    const StyleResolverState& state() const { ASSERT(m_state); return *m_state; }

private:
    StyleResolverState* m_state;
};

} // namespace blink

#endif // InterpolationEnvironment_h
