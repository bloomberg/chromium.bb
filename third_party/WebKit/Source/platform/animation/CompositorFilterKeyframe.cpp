// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterKeyframe.h"

namespace blink {

CompositorFilterKeyframe::CompositorFilterKeyframe(double time, PassOwnPtr<CompositorFilterOperations> value)
    : m_time(time)
    , m_value(std::move(value))
{
}

CompositorFilterKeyframe::~CompositorFilterKeyframe()
{
    m_value.clear();
}

} // namespace blink
