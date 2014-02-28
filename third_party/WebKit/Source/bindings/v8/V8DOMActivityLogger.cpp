// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/V8DOMActivityLogger.h"

#include "bindings/v8/V8Binding.h"
#include "wtf/HashMap.h"
#include "wtf/MainThread.h"

namespace WebCore {

typedef HashMap<int, OwnPtr<V8DOMActivityLogger>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int> > DOMActivityLoggerMap;

static DOMActivityLoggerMap& domActivityLoggers()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(DOMActivityLoggerMap, map, ());
    return map;
}

void V8DOMActivityLogger::setActivityLogger(int worldId, PassOwnPtr<V8DOMActivityLogger> logger)
{
    domActivityLoggers().set(worldId, logger);
}

V8DOMActivityLogger* V8DOMActivityLogger::activityLogger(int worldId)
{
    DOMActivityLoggerMap& loggers = domActivityLoggers();
    DOMActivityLoggerMap::iterator it = loggers.find(worldId);
    return it == loggers.end() ? 0 : it->value.get();
}

} // namespace WebCore
