// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/platform/WebSchedulerProxy.h"

#include "platform/scheduler/Scheduler.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
namespace {

void runTask(PassOwnPtr<WebThread::Task> task)
{
    task->run();
}

} // namespace

WebSchedulerProxy WebSchedulerProxy::create()
{
    return WebSchedulerProxy();
}

WebSchedulerProxy::WebSchedulerProxy()
    : m_scheduler(blink::Scheduler::shared())
{
    ASSERT(m_scheduler);
}

WebSchedulerProxy::~WebSchedulerProxy()
{
}

void WebSchedulerProxy::postInputTask(WebThread::Task* task)
{
    m_scheduler->postInputTask(bind(&runTask, adoptPtr(task)));
}

void WebSchedulerProxy::postCompositorTask(WebThread::Task* task)
{
    m_scheduler->postCompositorTask(bind(&runTask, adoptPtr(task)));
}

} // namespace blink
