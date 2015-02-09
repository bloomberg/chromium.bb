// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/CancellableTaskFactory.h"

#include "public/platform/Platform.h"
#include "wtf/InstanceCounter.h"

namespace blink {

void CancellableTaskFactory::cancel()
{
    m_weakPtrFactory.revokeAll();
}

WebThread::Task* CancellableTaskFactory::task()
{
    cancel();
    return new CancellableTask(m_weakPtrFactory.createWeakPtr());
}

void CancellableTaskFactory::CancellableTask::run()
{
    if (m_weakPtr.get()) {
        Closure* closure = m_weakPtr->m_closure.get();
        m_weakPtr->m_weakPtrFactory.revokeAll();
        (*closure)();
    }
}

} // namespace blink
