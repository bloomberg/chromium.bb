/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "modules/webaudio/OfflineAudioContext.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/webaudio/OfflineAudioCompletionEvent.h"
#include "modules/webaudio/OfflineAudioDestinationNode.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/audio/AudioUtilities.h"
#include "public/platform/Platform.h"


namespace blink {

OfflineAudioContext* OfflineAudioContext::create(ExecutionContext* context, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    // FIXME: add support for workers.
    if (!context || !context->isDocument()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "Workers are not supported.");
        return nullptr;
    }

    Document* document = toDocument(context);

    if (!numberOfFrames) {
        exceptionState.throwDOMException(SyntaxError, "number of frames cannot be zero.");
        return nullptr;
    }

    if (numberOfChannels > AbstractAudioContext::maxNumberOfChannels()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange<unsigned>(
                "number of channels",
                numberOfChannels,
                0,
                ExceptionMessages::InclusiveBound,
                AbstractAudioContext::maxNumberOfChannels(),
                ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    if (!AudioUtilities::isValidAudioBufferSampleRate(sampleRate)) {
        exceptionState.throwDOMException(
            IndexSizeError,
            ExceptionMessages::indexOutsideRange(
                "sampleRate", sampleRate,
                AudioUtilities::minAudioBufferSampleRate(), ExceptionMessages::InclusiveBound,
                AudioUtilities::maxAudioBufferSampleRate(), ExceptionMessages::InclusiveBound));
        return nullptr;
    }

    OfflineAudioContext* audioContext = new OfflineAudioContext(document, numberOfChannels, numberOfFrames, sampleRate);

    if (!audioContext->destination()) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "OfflineAudioContext(" + String::number(numberOfChannels)
            + ", " + String::number(numberOfFrames)
            + ", " + String::number(sampleRate)
            + ")");
    }

    audioContext->suspendIfNeeded();
    return audioContext;
}

OfflineAudioContext::OfflineAudioContext(Document* document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : AbstractAudioContext(document, numberOfChannels, numberOfFrames, sampleRate)
    , m_isRenderingStarted(false)
    , m_totalRenderFrames(numberOfFrames)
{
}

OfflineAudioContext::~OfflineAudioContext()
{
}

DEFINE_TRACE(OfflineAudioContext)
{
    visitor->trace(m_completeResolver);
    AbstractAudioContext::trace(visitor);
}

bool OfflineAudioContext::shouldSuspendNow()
{
    ASSERT(!isMainThread());

    // If there is any scheduled suspend at |currentSampleFrame|, the context
    // should be suspended.
    if (!m_scheduledSuspends.contains(currentSampleFrame()))
        return false;

    return true;
}

void OfflineAudioContext::resolvePendingSuspendPromises()
{
    ASSERT(!isMainThread());

    // Find a suspend scheduled at |currentSampleFrame| and resolve the
    // associated promise on the main thread.
    SuspendContainerMap::iterator it = m_scheduledSuspends.find(currentSampleFrame());
    if (it == m_scheduledSuspends.end())
        return;

    ScheduledSuspendContainer* suspendContainer = it->value;
    m_scheduledSuspends.remove(it);

    // Passing a raw pointer |suspendContainer| is safe here because it is
    // created/managed/destructed on the main thread. Also it is guaranteed to
    // be alive until the task is finished somewhere else.
    Platform::current()->mainThread()->postTask(FROM_HERE,
        threadSafeBind(&OfflineAudioContext::resolveSuspendContainerOnMainThread, this, AllowCrossThreadAccess(suspendContainer)));
}

void OfflineAudioContext::fireCompletionEvent()
{
    ASSERT(isMainThread());

    // We set the state to closed here so that the oncomplete event handler sees
    // that the context has been closed.
    setContextState(Closed);

    AudioBuffer* renderedBuffer = renderTarget();

    ASSERT(renderedBuffer);
    if (!renderedBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (executionContext()) {
        // Call the offline rendering completion event listener and resolve the
        // promise too.
        dispatchEvent(OfflineAudioCompletionEvent::create(renderedBuffer));
        m_completeResolver->resolve(renderedBuffer);
    } else {
        // The resolver should be rejected when the execution context is gone.
        m_completeResolver->reject(DOMException::create(InvalidStateError,
            "the execution context does not exist"));
    }
}

ScriptPromise OfflineAudioContext::startOfflineRendering(ScriptState* scriptState)
{
    ASSERT(isMainThread());

    // Calling close() on an OfflineAudioContext is not supported/allowed,
    // but it might well have been closed by its execution context.
    if (isContextClosed()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot call startRendering on an OfflineAudioContext in a closed state."));
    }

    if (m_completeResolver) {
        // Can't call startRendering more than once.  Return a rejected promise now.
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot call startRendering more than once"));
    }

    // If the context is not in the suspended state, reject the promise.
    if (contextState() != AudioContextState::Suspended) {
        return ScriptPromise::rejectWithDOMException(
            scriptState,
            DOMException::create(
                InvalidStateError,
                "cannot startRendering when an OfflineAudioContext is not in a suspended state"));
    }

    m_completeResolver = ScriptPromiseResolver::create(scriptState);

    // Start rendering and return the promise.
    ASSERT(!m_isRenderingStarted);
    m_isRenderingStarted = true;
    setContextState(Running);
    destinationHandler().startRendering();

    return m_completeResolver->promise();
}

ScriptPromise OfflineAudioContext::closeContext(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(InvalidAccessError, "cannot close an OfflineAudioContext."));
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* scriptState)
{
    // This CANNOT be called on OfflineAudioContext; it is to implement the pure
    // virtual interface from AbstractAudioContext.
    RELEASE_ASSERT_NOT_REACHED();

    return ScriptPromise();
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* scriptState, double when)
{
    ASSERT(isMainThread());

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // The render thread does not exist; reject the promise.
    if (!destinationHandler().offlineRenderThread()) {
        resolver->reject(DOMException::create(InvalidStateError,
            "the rendering is already finished"));
        return promise;
    }

    // The specified suspend time is negative; reject the promise.
    if (when < 0) {
        resolver->reject(DOMException::create(InvalidStateError,
            "negative suspend time (" + String::number(when) + ") is not allowed"));
        return promise;
    }

    // Quantize the suspend time to the render quantum boundary.
    size_t quantizedFrame = destinationHandler().quantizeTimeToRenderQuantum(when);

    // The suspend time should be earlier than the total render frame. If the
    // requested suspension time is equal to the total render frame, the promise
    // will be rejected.
    if (m_totalRenderFrames <= quantizedFrame) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot schedule a suspend at frame " + String::number(quantizedFrame) +
            " (" + String::number(when) + " seconds) " +
            "because it is greater than or equal to the total render duration of " +
            String::number(m_totalRenderFrames) + " frames"));
        return promise;
    }

    ScheduledSuspendContainer* suspendContainer = ScheduledSuspendContainer::create(when, quantizedFrame, resolver);

    // Validate the suspend and append if necessary on the render thread.
    // Note that passing a raw pointer |suspendContainer| is safe here as well
    // with the same reason in |resolvePendingSuspendPromises|.
    destinationHandler().offlineRenderThread()->postTask(FROM_HERE,
        threadSafeBind(&OfflineAudioContext::validateSuspendContainerOnRenderThread, this, AllowCrossThreadAccess(suspendContainer)));

    return promise;
}

ScriptPromise OfflineAudioContext::resumeContext(ScriptState* scriptState)
{
    ASSERT(isMainThread());

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // If the context is not in a suspended state, reject the promise.
    if (contextState() != AudioContextState::Suspended) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot resume a context that is not suspended"));
        return promise;
    }

    // If the rendering has not started, reject the promise.
    if (!m_isRenderingStarted) {
        resolver->reject(DOMException::create(InvalidStateError,
            "cannot resume a context that has not started"));
        return promise;
    }

    // If the context is suspended, resume rendering by setting the state to
    // "Running." and calling startRendering(). Note that resuming is possible
    // only after the rendering started.
    setContextState(Running);
    destinationHandler().startRendering();

    // Resolve the promise immediately.
    resolver->resolve();

    return promise;
}

OfflineAudioDestinationHandler& OfflineAudioContext::destinationHandler()
{
    return static_cast<OfflineAudioDestinationHandler&>(destination()->audioDestinationHandler());
}

void OfflineAudioContext::validateSuspendContainerOnRenderThread(ScheduledSuspendContainer* suspendContainer)
{
    ASSERT(!isMainThread());

    bool shouldBeRejected = false;

    // The specified suspend time is in the past; reject the promise. We cannot
    // reject the promise in here, so set the error message before posting the
    // rejection task to the main thread.
    if (suspendContainer->suspendFrame() < currentSampleFrame()) {
        shouldBeRejected = true;
        suspendContainer->setErrorMessageForRejection(InvalidStateError,
            "cannot schedule a suspend at frame " +
            String::number(suspendContainer->suspendFrame()) + " (" +
            String::number(suspendContainer->suspendTime()) +
            " seconds) because it is earlier than the current frame of " +
            String::number(currentSampleFrame()) + " (" +
            String::number(currentTime()) + " seconds)");
    } else if (m_scheduledSuspends.contains(suspendContainer->suspendFrame())) {
        // If there is a duplicate suspension at the same quantized frame,
        // reject the promise. The rejection of promise should happen in the
        // main thread, so we post a task to it. Here we simply set the error
        // message and post a task to the main thread.
        shouldBeRejected = true;
        suspendContainer->setErrorMessageForRejection(InvalidStateError,
            "cannot schedule more than one suspend at frame " +
            String::number(suspendContainer->suspendFrame()) + " (" +
            String::number(suspendContainer->suspendTime()) + " seconds)");
    }

    // Reject the promise if necessary on the main thread.
    // Note that passing a raw pointer |suspendContainer| is safe here as well
    // with the same reason in |resolvePendingSuspendPromises|.
    if (shouldBeRejected) {
        Platform::current()->mainThread()->postTask(FROM_HERE,
            threadSafeBind(&OfflineAudioContext::rejectSuspendContainerOnMainThread, this, AllowCrossThreadAccess(suspendContainer)));
        return;
    }

    // If the validation check passes, we can add the container safely here in
    // the render thread.
    m_scheduledSuspends.add(suspendContainer->suspendFrame(), suspendContainer);
}

void OfflineAudioContext::rejectSuspendContainerOnMainThread(ScheduledSuspendContainer* suspendContainer)
{
    ASSERT(isMainThread());

    suspendContainer->rejectPromise();
}

void OfflineAudioContext::resolveSuspendContainerOnMainThread(ScheduledSuspendContainer* suspendContainer)
{
    ASSERT(isMainThread());

    // Suspend the context first. This will fire onstatechange event.
    setContextState(Suspended);

    suspendContainer->resolvePromise();
}

ScheduledSuspendContainer::ScheduledSuspendContainer(double suspendTime, size_t suspendFrame, PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
    : m_suspendTime(suspendTime)
    , m_suspendFrame(suspendFrame)
    , m_resolver(resolver)
{
    ASSERT(isMainThread());
}

ScheduledSuspendContainer::~ScheduledSuspendContainer()
{
    ASSERT(isMainThread());
}

ScheduledSuspendContainer* ScheduledSuspendContainer::create(double suspendTime, size_t suspendFrame, PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
{
    return new ScheduledSuspendContainer(suspendTime, suspendFrame, resolver);
}

void ScheduledSuspendContainer::setErrorMessageForRejection(ExceptionCode errorCode, const String& errorMessage)
{
    ASSERT(!isMainThread());

    m_errorCode = errorCode;

    // |errorMessage| is created on the render thread, but its actual usage is
    // in the main thread. So we need to be thread-safe on this.
    m_errorMessage = errorMessage.isolatedCopy();
}

void ScheduledSuspendContainer::resolvePromise()
{
    ASSERT(isMainThread());

    m_resolver->resolve();
}

void ScheduledSuspendContainer::rejectPromise()
{
    ASSERT(isMainThread());
    ASSERT(m_errorCode);
    ASSERT(m_errorMessage);

    m_resolver->reject(DOMException::create(m_errorCode, m_errorMessage));
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
