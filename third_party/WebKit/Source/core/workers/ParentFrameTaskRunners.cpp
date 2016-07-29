// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ParentFrameTaskRunners.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "wtf/Assertions.h"

namespace blink {

ParentFrameTaskRunners::~ParentFrameTaskRunners()
{
}

WebTaskRunner* ParentFrameTaskRunners::getUnthrottledTaskRunner()
{
    return m_unthrottledTaskRunner.get();
}

WebTaskRunner* ParentFrameTaskRunners::getTimerTaskRunner()
{
    return m_timerTaskRunner.get();
}

WebTaskRunner* ParentFrameTaskRunners::getLoadingTaskRunner()
{
    return m_loadingTaskRunner.get();
}

ParentFrameTaskRunners::ParentFrameTaskRunners(Document* document)
    : m_unthrottledTaskRunner(TaskRunnerHelper::getUnthrottledTaskRunner(document)->clone())
    , m_timerTaskRunner(TaskRunnerHelper::getTimerTaskRunner(document)->clone())
    , m_loadingTaskRunner(TaskRunnerHelper::getLoadingTaskRunner(document)->clone())
{
    DCHECK(document->isContextThread());
}

} // namespace blink
