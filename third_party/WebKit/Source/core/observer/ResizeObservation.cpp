// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObservation.h"

#include "core/dom/Element.h"
#include "core/observer/ResizeObserver.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target, ResizeObserver* observer)
    : m_target(target)
    , m_observer(observer)
{
    DCHECK(m_target);
}

DEFINE_TRACE(ResizeObservation)
{
    visitor->trace(m_target);
    visitor->trace(m_observer);
}

} // namespace blink
