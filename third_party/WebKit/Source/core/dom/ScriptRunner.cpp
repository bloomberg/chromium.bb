/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/ScriptRunner.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptLoader.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"

namespace blink {

ScriptRunner::ScriptRunner(Document* document)
    : m_document(document)
    , m_taskRunner(Platform::current()->currentThread()->scheduler()->loadingTaskRunner())
    , m_numberOfInOrderScriptsWithPendingNotification(0)
    , m_isSuspended(false)
#if !ENABLE(OILPAN)
    , m_weakPointerFactoryForTasks(this)
#endif
{
    ASSERT(document);
#ifndef NDEBUG
    m_hasEverBeenSuspended = false;
#endif
}

ScriptRunner::~ScriptRunner()
{
#if !ENABLE(OILPAN)
    dispose();
#endif
}

class ScriptRunner::Task : public WebTaskRunner::Task {
    WTF_MAKE_NONCOPYABLE(Task);

public:
    explicit Task(WeakPtrWillBeRawPtr<ScriptRunner> scriptRunner)
        : m_scriptRunner(scriptRunner)
    {
    }

    virtual ~Task() { };

    void run() override
    {
        if (!m_scriptRunner)
            return;
        m_scriptRunner->executeTask();
    }

private:
    WeakPtrWillBeWeakPersistent<ScriptRunner> m_scriptRunner;
};

#if !ENABLE(OILPAN)
void ScriptRunner::dispose()
{
    // Make sure that ScriptLoaders don't keep their PendingScripts alive.
    for (ScriptLoader* scriptLoader : m_pendingInOrderScripts)
        scriptLoader->detach();
    for (ScriptLoader* scriptLoader : m_pendingAsyncScripts)
        scriptLoader->detach();
    for (ScriptLoader* scriptLoader : m_inOrderScriptsToExecuteSoon)
        scriptLoader->detach();
    for (ScriptLoader* scriptLoader : m_asyncScriptsToExecuteSoon)
        scriptLoader->detach();

    m_pendingInOrderScripts.clear();
    m_pendingAsyncScripts.clear();
    m_inOrderScriptsToExecuteSoon.clear();
    m_asyncScriptsToExecuteSoon.clear();
    m_numberOfInOrderScriptsWithPendingNotification = 0;
}
#endif

void ScriptRunner::queueScriptForExecution(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    ASSERT(scriptLoader);
    m_document->incrementLoadEventDelayCount();
    switch (executionType) {
    case ASYNC_EXECUTION:
        m_pendingAsyncScripts.add(scriptLoader);
        break;

    case IN_ORDER_EXECUTION:
        m_pendingInOrderScripts.append(scriptLoader);
        m_numberOfInOrderScriptsWithPendingNotification++;
        break;
    }
}

void ScriptRunner::postTask(const WebTraceLocation& webTraceLocation)
{
    // TODO(altimin): Replace all this with `new Task(this)` when Oilpan is here.
    WeakPtrWillBeRawPtr<ScriptRunner> scriptRunnerForTask;
#if !ENABLE(OILPAN)
    scriptRunnerForTask = m_weakPointerFactoryForTasks.createWeakPtr();
#else
    scriptRunnerForTask = this;
#endif
    m_taskRunner->postTask(webTraceLocation, new Task(scriptRunnerForTask));
}

void ScriptRunner::suspend()
{
#ifndef NDEBUG
    m_hasEverBeenSuspended = true;
#endif

    m_isSuspended = true;
}

void ScriptRunner::resume()
{
    ASSERT(m_isSuspended);

    m_isSuspended = false;

    for (size_t i = 0; i < m_asyncScriptsToExecuteSoon.size(); ++i) {
        postTask(BLINK_FROM_HERE);
    }
    for (size_t i = 0; i < m_inOrderScriptsToExecuteSoon.size(); ++i) {
        postTask(BLINK_FROM_HERE);
    }
}

void ScriptRunner::notifyScriptReady(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(scriptLoader);
    switch (executionType) {
    case ASYNC_EXECUTION:
        // RELEASE_ASSERT makes us crash in a controlled way in error cases
        // where the ScriptLoader is associated with the wrong ScriptRunner
        // (otherwise we'd cause a use-after-free in ~ScriptRunner when it tries
        // to detach).
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_pendingAsyncScripts.contains(scriptLoader));

        m_pendingAsyncScripts.remove(scriptLoader);
        m_asyncScriptsToExecuteSoon.append(scriptLoader);

        postTask(BLINK_FROM_HERE);

        break;

    case IN_ORDER_EXECUTION:
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_numberOfInOrderScriptsWithPendingNotification > 0);
        m_numberOfInOrderScriptsWithPendingNotification--;

        while (!m_pendingInOrderScripts.isEmpty() && m_pendingInOrderScripts.first()->isReady()) {
            m_inOrderScriptsToExecuteSoon.append(m_pendingInOrderScripts.takeFirst());
            postTask(BLINK_FROM_HERE);
        }

        break;
    }
}

bool ScriptRunner::removePendingInOrderScript(ScriptLoader* scriptLoader)
{
    for (auto it = m_pendingInOrderScripts.begin(); it != m_pendingInOrderScripts.end(); ++it) {
        if (*it == scriptLoader) {
            m_pendingInOrderScripts.remove(it);
            RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_numberOfInOrderScriptsWithPendingNotification > 0);
            m_numberOfInOrderScriptsWithPendingNotification--;
            return true;
        }
    }
    return false;
}

void ScriptRunner::notifyScriptLoadError(ScriptLoader* scriptLoader, ExecutionType executionType)
{
    switch (executionType) {
    case ASYNC_EXECUTION: {
        // RELEASE_ASSERT makes us crash in a controlled way in error cases
        // where the ScriptLoader is associated with the wrong ScriptRunner
        // (otherwise we'd cause a use-after-free in ~ScriptRunner when it tries
        // to detach).
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_pendingAsyncScripts.contains(scriptLoader));
        m_pendingAsyncScripts.remove(scriptLoader);
        break;
    }
    case IN_ORDER_EXECUTION:
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(removePendingInOrderScript(scriptLoader));
        break;
    }
    scriptLoader->detach();
    m_document->decrementLoadEventDelayCount();
}

void ScriptRunner::movePendingScript(Document& oldDocument, Document& newDocument, ScriptLoader* scriptLoader)
{
    RefPtrWillBeRawPtr<Document> newContextDocument = newDocument.contextDocument().get();
    if (!newContextDocument) {
        // Document's contextDocument() method will return no Document if the
        // following conditions both hold:
        //
        //   - The Document wasn't created with an explicit context document
        //     and that document is otherwise kept alive.
        //   - The Document itself is detached from its frame.
        //
        // The script element's loader is in that case moved to document() and
        // its script runner, which is the non-null Document that contextDocument()
        // would return if not detached.
        ASSERT(!newDocument.frame());
        newContextDocument = &newDocument;
    }
    RefPtrWillBeRawPtr<Document> oldContextDocument = oldDocument.contextDocument().get();
    if (!oldContextDocument) {
        ASSERT(!oldDocument.frame());
        oldContextDocument = &oldDocument;
    }
    if (oldContextDocument != newContextDocument)
        oldContextDocument->scriptRunner()->movePendingScript(newContextDocument->scriptRunner(), scriptLoader);
}

void ScriptRunner::movePendingScript(ScriptRunner* newRunner, ScriptLoader* scriptLoader)
{
    if (m_pendingAsyncScripts.contains(scriptLoader)) {
        newRunner->queueScriptForExecution(scriptLoader, ASYNC_EXECUTION);
        m_pendingAsyncScripts.remove(scriptLoader);
        m_document->decrementLoadEventDelayCount();
        return;
    }
    if (removePendingInOrderScript(scriptLoader)) {
        newRunner->queueScriptForExecution(scriptLoader, IN_ORDER_EXECUTION);
        m_document->decrementLoadEventDelayCount();
    }
}

// Returns true if task was run, and false otherwise.
bool ScriptRunner::executeTaskFromQueue(WillBeHeapDeque<RawPtrWillBeMember<ScriptLoader>>* taskQueue)
{
    if (taskQueue->isEmpty())
        return false;
    taskQueue->takeFirst()->execute();

    m_document->decrementLoadEventDelayCount();
    return true;
}

void ScriptRunner::executeTask()
{
    if (m_isSuspended)
        return;

    if (executeTaskFromQueue(&m_asyncScriptsToExecuteSoon))
        return;

    if (executeTaskFromQueue(&m_inOrderScriptsToExecuteSoon))
        return;

#ifndef NDEBUG
    // Extra tasks should be posted only when we resume after suspending.
    ASSERT(m_hasEverBeenSuspended);
#endif
}

DEFINE_TRACE(ScriptRunner)
{
#if ENABLE(OILPAN)
    visitor->trace(m_document);
    visitor->trace(m_pendingInOrderScripts);
    visitor->trace(m_pendingAsyncScripts);
    visitor->trace(m_asyncScriptsToExecuteSoon);
    visitor->trace(m_inOrderScriptsToExecuteSoon);
#endif
}

} // namespace blink
