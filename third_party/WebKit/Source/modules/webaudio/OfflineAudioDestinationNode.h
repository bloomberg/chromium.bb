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
#include "public/platform/WebThread.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class AudioBus;
class AudioContext;

class OfflineAudioDestinationNode final : public AudioDestinationNode {
public:
    static OfflineAudioDestinationNode* create(AudioContext* context, AudioBuffer* renderTarget)
    {
        return new OfflineAudioDestinationNode(context, renderTarget);
    }

    virtual ~OfflineAudioDestinationNode();

    // AudioNode
    virtual void dispose() override;
    virtual void initialize() override;
    virtual void uninitialize() override;

    // AudioDestinationNode
    virtual void startRendering() override;

    virtual float sampleRate()  const override { return m_renderTarget->sampleRate(); }

    virtual void trace(Visitor*) override;

private:
    OfflineAudioDestinationNode(AudioContext*, AudioBuffer* renderTarget);

    // This AudioNode renders into this AudioBuffer.
    Member<AudioBuffer> m_renderTarget;
    // Temporary AudioBus for each render quantum.
    RefPtr<AudioBus> m_renderBus;

    // Rendering thread.
    OwnPtr<WebThread> m_renderThread;
    bool m_startedRendering;
    void offlineRender();

    // For completion callback on main thread.
    void notifyComplete();
};

} // namespace blink

#endif // OfflineAudioDestinationNode_h
