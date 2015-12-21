// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

#include "platform/Task.h"

namespace blink {

void WebTaskRunner::postTask(const WebTraceLocation& location, PassOwnPtr<ClosureTask> task)
{
    postTask(std::move(location), new blink::Task(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, PassOwnPtr <ClosureTask> task, long long delayMs)
{
    postDelayedTask(location, new blink::Task(std::move(task)), delayMs);
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, PassOwnPtr <ClosureTask> task, double delayMs)
{
    postDelayedTask(location, new blink::Task(std::move(task)), delayMs);
}

} // namespace blink
