// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/LocalFrameLifecycleNotifier.h"

#include "core/frame/FrameDestructionObserver.h"

namespace blink {

void LocalFrameLifecycleNotifier::notifyWillDetachFrameHost()
{
    TemporaryChange<IterationType> scope(m_iterating, IteratingOverAll);
    for (FrameDestructionObserver* observer : m_observers)
        observer->willDetachFrameHost();
}

} // namespace blink
