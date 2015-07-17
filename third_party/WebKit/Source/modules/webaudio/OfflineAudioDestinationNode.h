/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#ifndef OfflineAudioDestinationNode_h
#define OfflineAudioDestinationNode_h

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "public/platform/WebThread.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class AbstractAudioContext;
class AudioBus;
class OfflineAudioContext;

class OfflineAudioDestinationHandler final : public AudioDestinationHandler {
public:
    static PassRefPtr<OfflineAudioDestinationHandler> create(AudioNode&, AudioBuffer* renderTarget);
    ~OfflineAudioDestinationHandler() override;

    // AudioHandler
    void dispose() override;
    void initialize() override;
    void uninitialize() override;

    OfflineAudioContext* context() const final;

    // Override the one from AudioContext. Create a rendering thread and
    // initiate rendering loop or resume rendering if already started and
    // suspended.
    void startRendering() override;

    // There is no support of the explicit stopping in OfflineAudioContext. This
    // is just to match the class method signature.
    void stopRendering() override;

    float sampleRate() const override { return m_renderTarget->sampleRate(); }

    // Return the frame position by quantizing the time (in seconds) and
    // rounding it down to the nearest render quantum boundary.
    size_t quantizeTimeToRenderQuantum(double when) const;

    WebThread* offlineRenderThread();

private:
    OfflineAudioDestinationHandler(AudioNode&, AudioBuffer* renderTarget);

    // For start/suspend/resume rendering in the destination node.
    void startOfflineRendering();
    void runOfflineRendering();
    void finishOfflineRendering();

    // For completion callback on main thread.
    void notifyComplete();

    // This AudioHandler renders into this AudioBuffer.
    // This Persistent doesn't make a reference cycle including the owner
    // OfflineAudioDestinationNode.
    Persistent<AudioBuffer> m_renderTarget;
    // Temporary AudioBus for each render quantum.
    RefPtr<AudioBus> m_renderBus;

    // Rendering thread.
    OwnPtr<WebThread> m_renderThread;

    size_t m_framesProcessed;
    size_t m_framesToProcess;

    bool m_isRenderingStarted;
};

class OfflineAudioDestinationNode final : public AudioDestinationNode {
public:
    static OfflineAudioDestinationNode* create(AbstractAudioContext*, AudioBuffer* renderTarget);

private:
    OfflineAudioDestinationNode(AbstractAudioContext&, AudioBuffer* renderTarget);
};

} // namespace blink

#endif // OfflineAudioDestinationNode_h
