// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/platform/WebSchedulerProxy.h"

#include "platform/TraceLocation.h"
#include "platform/scheduler/Scheduler.h"
#include "public/platform/WebTraceLocation.h"
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
    : m_scheduler(Scheduler::shared())
{
    ASSERT(m_scheduler);
}

WebSchedulerProxy::~WebSchedulerProxy()
{
}

void WebSchedulerProxy::postInputTask(const WebTraceLocation& webLocation, WebThread::Task* task)
{
    TraceLocation location(webLocation.functionName(), webLocation.fileName());
    m_scheduler->postInputTask(location, bind(&runTask, adoptPtr(task)));
}

void WebSchedulerProxy::postCompositorTask(const WebTraceLocation& webLocation, WebThread::Task* task)
{
    TraceLocation location(webLocation.functionName(), webLocation.fileName());
    m_scheduler->postCompositorTask(location, bind(&runTask, adoptPtr(task)));
}

void WebSchedulerProxy::didReceiveInputEvent()
{
    m_scheduler->didReceiveInputEvent();
}

} // namespace blink
