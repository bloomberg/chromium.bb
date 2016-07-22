// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserverController.h"

#include "core/observer/ResizeObserver.h"


namespace blink {

ResizeObserverController::ResizeObserverController()
{
}

void ResizeObserverController::addObserver(ResizeObserver& observer)
{
    m_observers.add(&observer);
}


DEFINE_TRACE(ResizeObserverController)
{
    visitor->trace(m_observers);
}

} // namespace blink
