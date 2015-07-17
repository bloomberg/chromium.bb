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

#ifndef OfflineAudioContext_h
#define OfflineAudioContext_h

#include "modules/ModulesExport.h"
#include "modules/webaudio/AbstractAudioContext.h"

#include "wtf/HashMap.h"

namespace blink {

class ExceptionState;
class ScheduledSuspendContainer;
class OfflineAudioDestinationHandler;

// The HashMap with 'zero' key is needed because |currentSampleFrame| can be
// zero. Also ScheduledSuspendContainer is a raw pointer (rather than RefPtr)
// because it must be guaranteed that the main thread keeps it alive while the
// render thread uses it.
using SuspendContainerMap = HashMap<size_t, ScheduledSuspendContainer*, DefaultHash<size_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;

class MODULES_EXPORT OfflineAudioContext final : public AbstractAudioContext {
    DEFINE_WRAPPERTYPEINFO();
public:
    static OfflineAudioContext* create(ExecutionContext*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState&);

    ~OfflineAudioContext() override;

    DECLARE_VIRTUAL_TRACE();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);

    // Check all the scheduled suspends if the context should suspend at
    // currentTime(). Then post tasks to resolve promises on the main thread
    // if necessary.
    bool shouldSuspendNow();

    // Clear suspensions marked as 'resolved' in the list.
    void resolvePendingSuspendPromises();

    // Fire completion event when the rendering is finished.
    void fireCompletionEvent();

    ScriptPromise startOfflineRendering(ScriptState*);

    ScriptPromise closeContext(ScriptState*) final;
    ScriptPromise suspendContext(ScriptState*, double) final;
    ScriptPromise resumeContext(ScriptState*) final;

    // This is to implement the pure virtual method from AbstractAudioContext.
    // CANNOT be called on OfflineAudioContext.
    ScriptPromise suspendContext(ScriptState*) final;

    bool hasRealtimeConstraint() final { return false; }

private:
    OfflineAudioContext(Document*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);

    // Fetch directly the destination handler.
    OfflineAudioDestinationHandler& destinationHandler();

    // Check a suspend container if it has a duplicate scheduled frame or
    // is behind the current frame. If the validation fails, post a task to the
    // main thread to reject the promise.
    void validateSuspendContainerOnRenderThread(ScheduledSuspendContainer*);

    // Reject a suspend container on the main thread when the validation fails.
    void rejectSuspendContainerOnMainThread(ScheduledSuspendContainer*);

    // Resolve a pending suspend container.
    void resolveSuspendContainerOnMainThread(ScheduledSuspendContainer*);

    SuspendContainerMap m_scheduledSuspends;
    RefPtrWillBeMember<ScriptPromiseResolver> m_completeResolver;

    // This flag is necessary to indicate the rendering has actually started.
    // Note that initial state of context is 'Suspended', which is the same
    // state when the context is suspended.
    bool m_isRenderingStarted;

    // Total render sample length.
    size_t m_totalRenderFrames;
};

// A container class for a pair of time information and the suspend promise
// resolver. This class does not need to be |ThreadSafeRefCounted| because it
// needs to be created and destructed in the main thread.
class ScheduledSuspendContainer {
public:
    static ScheduledSuspendContainer* create(double suspendTime, size_t suspendFrame, PassRefPtrWillBeRawPtr<ScriptPromiseResolver>);
    ~ScheduledSuspendContainer();

    double suspendTime() const { return m_suspendTime; }
    size_t suspendFrame() const { return m_suspendFrame; }

    // Set the error message for the reason of rejection on the render thread
    // before sending it to the main thread.
    void setErrorMessageForRejection(ExceptionCode, const String&);

    // {Resolve, Reject} the promise resolver on the main thread.
    void resolvePromise();
    void rejectPromise();

private:
    ScheduledSuspendContainer(double suspendTime, size_t suspendFrame, PassRefPtrWillBeRawPtr<ScriptPromiseResolver>);

    // Actual suspend time before the quantization by render quantum frame.
    double m_suspendTime;

    // Suspend sample frame. This is quantized by the render quantum size.
    size_t m_suspendFrame;

    // Associated promise resolver.
    RefPtrWillBePersistent<ScriptPromiseResolver> m_resolver;

    ExceptionCode m_errorCode;
    String m_errorMessage;
};

} // namespace blink

#endif // OfflineAudioContext_h
