// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/V8DOMActivityLogger.h"

#include "bindings/v8/V8Binding.h"

namespace WebCore {

void V8DOMActivityLogger::setActivityLogger(int worldId, PassOwnPtr<V8DOMActivityLogger> logger)
{
    DOMWrapperWorld* world = DOMWrapperWorld::from(worldId);
    if (world)
        world->setActivityLogger(logger);
}

V8DOMActivityLogger* V8DOMActivityLogger::activityLogger(int worldId)
{
    DOMWrapperWorld* world = DOMWrapperWorld::from(worldId);
    return world ? world->activityLogger() : 0;
}

} // namespace WebCore
