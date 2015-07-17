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

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "modules/webaudio/OfflineAudioDestinationNode.h"

#include "core/dom/CrossThreadTask.h"
#include "modules/webaudio/AbstractAudioContext.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/Task.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/HRTFDatabaseLoader.h"
#include "public/platform/Platform.h"
#include <algorithm>

namespace blink {

const size_t renderQuantumSize = 128;

OfflineAudioDestinationHandler::OfflineAudioDestinationHandler(AudioNode& node, AudioBuffer* renderTarget)
    : AudioDestinationHandler(node, renderTarget->sampleRate())
    , m_renderTarget(renderTarget)
    , m_renderThread(adoptPtr(Platform::current()->createThread("offline audio renderer")))
    , m_framesProcessed(0)
    , m_framesToProcess(0)
    , m_isRenderingStarted(false)
{
    m_renderBus = AudioBus::create(renderTarget->numberOfChannels(), renderQuantumSize);
}

PassRefPtr<OfflineAudioDestinationHandler> OfflineAudioDestinationHandler::create(AudioNode& node, AudioBuffer* renderTarget)
{
    return adoptRef(new OfflineAudioDestinationHandler(node, renderTarget));
}

OfflineAudioDestinationHandler::~OfflineAudioDestinationHandler()
{
    ASSERT(!isInitialized());
}

void OfflineAudioDestinationHandler::dispose()
{
    uninitialize();
    AudioDestinationHandler::dispose();
}

void OfflineAudioDestinationHandler::initialize()
{
    if (isInitialized())
        return;

    AudioHandler::initialize();
}

void OfflineAudioDestinationHandler::uninitialize()
{
    if (!isInitialized())
        return;

    if (m_renderThread)
        m_renderThread.clear();

    AudioHandler::uninitialize();
}

OfflineAudioContext* OfflineAudioDestinationHandler::context() const
{
    return static_cast<OfflineAudioContext*>(m_context);
}

void OfflineAudioDestinationHandler::startRendering()
{
    ASSERT(isMainThread());
    ASSERT(m_renderTarget);
    ASSERT(m_renderThread);

    if (!m_renderTarget)
        return;

    // Rendering was not started. Starting now.
    if (!m_isRenderingStarted) {
        m_renderThread->postTask(FROM_HERE, new Task(threadSafeBind(&OfflineAudioDestinationHandler::startOfflineRendering, this)));
        m_isRenderingStarted = true;
        return;
    }

    // Rendering is already started, which implicitly means we resume the
    // rendering by calling |runOfflineRendering| on m_renderThread.
    m_renderThread->postTask(FROM_HERE, threadSafeBind(&OfflineAudioDestinationHandler::runOfflineRendering, this));
}

void OfflineAudioDestinationHandler::stopRendering()
{
    ASSERT_NOT_REACHED();
}

size_t OfflineAudioDestinationHandler::quantizeTimeToRenderQuantum(double when) const
{
    ASSERT(when >= 0);

    size_t whenAsFrame = when * sampleRate();
    return whenAsFrame - (whenAsFrame % renderQuantumSize);
}

WebThread* OfflineAudioDestinationHandler::offlineRenderThread()
{
    ASSERT(m_renderThread);

    return m_renderThread.get();
}

void OfflineAudioDestinationHandler::startOfflineRendering()
{
    ASSERT(!isMainThread());

    ASSERT(m_renderBus);
    if (!m_renderBus)
        return;

    bool isAudioContextInitialized = context()->isInitialized();
    ASSERT(isAudioContextInitialized);
    if (!isAudioContextInitialized)
        return;

    bool channelsMatch = m_renderBus->numberOfChannels() == m_renderTarget->numberOfChannels();
    ASSERT(channelsMatch);
    if (!channelsMatch)
        return;

    bool isRenderBusAllocated = m_renderBus->length() >= renderQuantumSize;
    ASSERT(isRenderBusAllocated);
    if (!isRenderBusAllocated)
        return;

    m_framesToProcess = m_renderTarget->length();

    // Start rendering.
    runOfflineRendering();
}

void OfflineAudioDestinationHandler::runOfflineRendering()
{
    ASSERT(!isMainThread());

    unsigned numberOfChannels = m_renderTarget->numberOfChannels();

    // If there is more to process and there is no suspension at the moment,
    // do continue to render quanta. If there is a suspend scheduled at the
    // current sample frame, stop the render loop and put the context into the
    // suspended state. Then calling OfflineAudioContext.resume() will pick up
    // the render loop again from where it was suspended.
    while (m_framesToProcess > 0 && !context()->shouldSuspendNow()) {

        // Render one render quantum. Note that this includes pre/post render
        // tasks from the online audio context.
        render(0, m_renderBus.get(), renderQuantumSize);

        size_t framesAvailableToCopy = std::min(m_framesToProcess, renderQuantumSize);

        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            const float* source = m_renderBus->channel(channelIndex)->data();
            float* destination = m_renderTarget->getChannelData(channelIndex)->data();
            memcpy(destination + m_framesProcessed, source, sizeof(float) * framesAvailableToCopy);
        }

        m_framesProcessed += framesAvailableToCopy;
        m_framesToProcess -= framesAvailableToCopy;
    }

    // Finish up the rendering loop if there is no more to process.
    if (m_framesToProcess <= 0) {
        ASSERT(m_framesToProcess == 0);
        finishOfflineRendering();
        return;
    }

    // Otherwise resolve pending suspend promises.
    context()->resolvePendingSuspendPromises();
}

void OfflineAudioDestinationHandler::finishOfflineRendering()
{
    ASSERT(!isMainThread());

    // Our work is done. Let the AbstractAudioContext know.
    if (context()->executionContext())
        context()->executionContext()->postTask(FROM_HERE, createCrossThreadTask(&OfflineAudioDestinationHandler::notifyComplete, this));
}

void OfflineAudioDestinationHandler::notifyComplete()
{
    // The AbstractAudioContext might be gone.
    if (context())
        context()->fireCompletionEvent();
}

// ----------------------------------------------------------------

OfflineAudioDestinationNode::OfflineAudioDestinationNode(AbstractAudioContext& context, AudioBuffer* renderTarget)
    : AudioDestinationNode(context)
{
    setHandler(OfflineAudioDestinationHandler::create(*this, renderTarget));
}

OfflineAudioDestinationNode* OfflineAudioDestinationNode::create(AbstractAudioContext* context, AudioBuffer* renderTarget)
{
    return new OfflineAudioDestinationNode(*context, renderTarget);
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
