// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/TaskRunnerHelper.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"

namespace blink {

WebTaskRunner* TaskRunnerHelper::getUnthrottledTaskRunner(LocalFrame* frame)
{
    return frame ? frame->frameScheduler()->unthrottledTaskRunner() : Platform::current()->currentThread()->getWebTaskRunner();
}

WebTaskRunner* TaskRunnerHelper::getTimerTaskRunner(LocalFrame* frame)
{
    return frame ? frame->frameScheduler()->timerTaskRunner() : Platform::current()->currentThread()->getWebTaskRunner();
}

WebTaskRunner* TaskRunnerHelper::getLoadingTaskRunner(LocalFrame* frame)
{
    return frame ? frame->frameScheduler()->loadingTaskRunner() : Platform::current()->currentThread()->getWebTaskRunner();
}

WebTaskRunner* TaskRunnerHelper::getUnthrottledTaskRunner(Document* document)
{
    return getUnthrottledTaskRunner(document ? document->frame() : nullptr);
}

WebTaskRunner* TaskRunnerHelper::getTimerTaskRunner(Document* document)
{
    return getTimerTaskRunner(document ? document->frame() : nullptr);
}

WebTaskRunner* TaskRunnerHelper::getLoadingTaskRunner(Document* document)
{
    return getLoadingTaskRunner(document ? document->frame() : nullptr);
}

} // namespace blink
