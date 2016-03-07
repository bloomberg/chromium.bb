// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

#include "platform/Task.h"

namespace blink {

void WebTaskRunner::postTask(const WebTraceLocation& location, PassOwnPtr<CrossThreadClosure> task)
{
    postTask(location, new CrossThreadTask(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, PassOwnPtr<CrossThreadClosure> task, long long delayMs)
{
    postDelayedTask(location, new CrossThreadTask(std::move(task)), delayMs);
}

void WebTaskRunner::postTask(const WebTraceLocation& location, PassOwnPtr<SameThreadClosure> task)
{
    postTask(location, new SameThreadTask(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, PassOwnPtr <SameThreadClosure> task, long long delayMs)
{
    postDelayedTask(location, new SameThreadTask(std::move(task)), delayMs);
}

} // namespace blink
