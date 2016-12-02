/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"

namespace blink {

MainThreadTaskRunner::MainThreadTaskRunner(ExecutionContext* context)
    : m_context(context),
      m_weakFactory(this),
      // Bind a WeakPtr now to avoid data races creating a WeakPtr inside
      // postTask.
      m_weakPtr(m_weakFactory.createWeakPtr()) {}

MainThreadTaskRunner::~MainThreadTaskRunner() {}

void MainThreadTaskRunner::postTask(const WebTraceLocation& location,
                                    std::unique_ptr<ExecutionContextTask> task,
                                    const String& taskNameForInstrumentation) {
  if (!taskNameForInstrumentation.isEmpty())
    InspectorInstrumentation::asyncTaskScheduled(
        m_context, taskNameForInstrumentation, task.get());
  const bool instrumenting = !taskNameForInstrumentation.isEmpty();
  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      location, crossThreadBind(&MainThreadTaskRunner::perform, m_weakPtr,
                                passed(std::move(task)), instrumenting));
}

void MainThreadTaskRunner::perform(std::unique_ptr<ExecutionContextTask> task,
                                   bool instrumenting) {
  // If the owner m_context is about to be swept then it
  // is no longer safe to access.
  if (ThreadHeap::willObjectBeLazilySwept(m_context.get()))
    return;

  InspectorInstrumentation::AsyncTask asyncTask(m_context, task.get(),
                                                instrumenting);
  task->performTask(m_context);
}

}  // namespace blink
