/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "modules/webaudio/AudioContext.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLMediaElement.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/audio/FFTFrame.h"
#include "platform/audio/HRTFPanner.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AnalyserNode.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferCallback.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BiquadFilterNode.h"
#include "modules/webaudio/ChannelMergerNode.h"
#include "modules/webaudio/ChannelSplitterNode.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/DefaultAudioDestinationNode.h"
#include "modules/webaudio/DelayNode.h"
#include "modules/webaudio/DynamicsCompressorNode.h"
#include "modules/webaudio/GainNode.h"
#include "modules/webaudio/MediaElementAudioSourceNode.h"
#include "modules/webaudio/MediaStreamAudioDestinationNode.h"
#include "modules/webaudio/MediaStreamAudioSourceNode.h"
#include "modules/webaudio/OfflineAudioCompletionEvent.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "modules/webaudio/OfflineAudioDestinationNode.h"
#include "modules/webaudio/OscillatorNode.h"
#include "modules/webaudio/PannerNode.h"
#include "modules/webaudio/PeriodicWave.h"
#include "modules/webaudio/ScriptProcessorNode.h"
#include "modules/webaudio/WaveShaperNode.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

#include "wtf/ArrayBuffer.h"
#include "wtf/Atomics.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

// FIXME: check the proper way to reference an undefined thread ID
const WTF::ThreadIdentifier UndefinedThreadIdentifier = 0xffffffff;

namespace blink {

bool AudioContext::isSampleRateRangeGood(float sampleRate)
{
    // FIXME: It would be nice if the minimum sample-rate could be less than 44.1KHz,
    // but that will require some fixes in HRTFPanner::fftSizeForSampleRate(), and some testing there.
    return sampleRate >= 44100 && sampleRate <= 96000;
}

// Don't allow more than this number of simultaneous AudioContexts talking to hardware.
const unsigned MaxHardwareContexts = 6;
unsigned AudioContext::s_hardwareContextCount = 0;

AudioContext* AudioContext::create(Document& document, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    if (s_hardwareContextCount >= MaxHardwareContexts) {
        exceptionState.throwDOMException(
            SyntaxError,
            "number of hardware contexts reached maximum (" + String::number(MaxHardwareContexts) + ").");
        return 0;
    }

    AudioContext* audioContext = adoptRefCountedGarbageCollectedWillBeNoop(new AudioContext(&document));
    audioContext->suspendIfNeeded();
    return audioContext;
}

// Constructor for rendering to the audio hardware.
AudioContext::AudioContext(Document* document)
    : ActiveDOMObject(document)
    , m_isStopScheduled(false)
    , m_isCleared(false)
    , m_isInitialized(false)
    , m_destinationNode(nullptr)
    , m_automaticPullNodesNeedUpdating(false)
    , m_connectionCount(0)
    , m_audioThread(0)
    , m_graphOwnerThread(UndefinedThreadIdentifier)
    , m_isOfflineContext(false)
{
    ScriptWrappable::init(this);

    m_destinationNode = DefaultAudioDestinationNode::create(this);

    initialize();
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: AudioContext::AudioContext() #%u\n", this, AudioContext::s_hardwareContextCount);
#endif
}

// Constructor for offline (non-realtime) rendering.
AudioContext::AudioContext(Document* document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : ActiveDOMObject(document)
    , m_isStopScheduled(false)
    , m_isCleared(false)
    , m_isInitialized(false)
    , m_destinationNode(nullptr)
    , m_automaticPullNodesNeedUpdating(false)
    , m_connectionCount(0)
    , m_audioThread(0)
    , m_graphOwnerThread(UndefinedThreadIdentifier)
    , m_isOfflineContext(true)
{
    ScriptWrappable::init(this);

    // Create a new destination for offline rendering.
    m_renderTarget = AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate);
    if (m_renderTarget.get())
        m_destinationNode = OfflineAudioDestinationNode::create(this, m_renderTarget.get());

    initialize();
}

AudioContext::~AudioContext()
{
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: AudioContext::~AudioContext()\n", this);
#endif
    // AudioNodes keep a reference to their context, so there should be no way to be in the destructor if there are still AudioNodes around.
    ASSERT(!m_isInitialized);
    ASSERT(!m_referencedNodes.size());
    ASSERT(!m_finishedNodes.size());
    ASSERT(!m_automaticPullNodes.size());
    if (m_automaticPullNodesNeedUpdating)
        m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());
    ASSERT(!m_renderingAutomaticPullNodes.size());
}

void AudioContext::initialize()
{
    if (isInitialized())
        return;

    FFTFrame::initialize();
    m_listener = AudioListener::create();

    if (m_destinationNode.get()) {
        m_destinationNode->initialize();

        if (!isOfflineContext()) {
            // This starts the audio thread. The destination node's provideInput() method will now be called repeatedly to render audio.
            // Each time provideInput() is called, a portion of the audio stream is rendered. Let's call this time period a "render quantum".
            // NOTE: for now default AudioContext does not need an explicit startRendering() call from JavaScript.
            // We may want to consider requiring it for symmetry with OfflineAudioContext.
            m_destinationNode->startRendering();
            ++s_hardwareContextCount;
        }

        m_isInitialized = true;
    }
}

void AudioContext::clear()
{
    // We need to run disposers before destructing m_contextGraphMutex.
    m_liveAudioSummingJunctions.clear();
    m_liveNodes.clear();
    m_destinationNode.clear();
    m_isCleared = true;
}

void AudioContext::uninitialize()
{
    ASSERT(isMainThread());

    if (!isInitialized())
        return;

    // This stops the audio thread and all audio rendering.
    m_destinationNode->uninitialize();

    if (!isOfflineContext()) {
        ASSERT(s_hardwareContextCount);
        --s_hardwareContextCount;
    }

    // Get rid of the sources which may still be playing.
    derefUnfinishedSourceNodes();

    m_isInitialized = false;
    clear();
}

void AudioContext::stop()
{
    // Usually ExecutionContext calls stop twice.
    if (m_isStopScheduled)
        return;
    m_isStopScheduled = true;

    // Don't call uninitialize() immediately here because the ExecutionContext is in the middle
    // of dealing with all of its ActiveDOMObjects at this point. uninitialize() can de-reference other
    // ActiveDOMObjects so let's schedule uninitialize() to be called later.
    // FIXME: see if there's a more direct way to handle this issue.
    callOnMainThread(bind(&AudioContext::uninitialize, this));
}

bool AudioContext::hasPendingActivity() const
{
    // According to spec AudioContext must die only after page navigates.
    return !m_isCleared;
}

AudioBuffer* AudioContext::createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState& exceptionState)
{
    return AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate, exceptionState);
}

void AudioContext::decodeAudioData(ArrayBuffer* audioData, PassOwnPtr<AudioBufferCallback> successCallback, PassOwnPtr<AudioBufferCallback> errorCallback, ExceptionState& exceptionState)
{
    if (!audioData) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid ArrayBuffer for audioData.");
        return;
    }
    m_audioDecoder.decodeAsync(audioData, sampleRate(), successCallback, errorCallback);
}

AudioBufferSourceNode* AudioContext::createBufferSource()
{
    ASSERT(isMainThread());
    AudioBufferSourceNode* node = AudioBufferSourceNode::create(this, m_destinationNode->sampleRate());

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls AudioContext::notifyNodeFinishedProcessing().
    refNode(node);

    return node;
}

MediaElementAudioSourceNode* AudioContext::createMediaElementSource(HTMLMediaElement* mediaElement, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    if (!mediaElement) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "invalid HTMLMedialElement.");
        return 0;
    }

    // First check if this media element already has a source node.
    if (mediaElement->audioSourceNode()) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "invalid HTMLMediaElement.");
        return 0;
    }

    MediaElementAudioSourceNode* node = MediaElementAudioSourceNode::create(this, mediaElement);

    mediaElement->setAudioSourceNode(node);

    refNode(node); // context keeps reference until node is disconnected
    return node;
}

MediaStreamAudioSourceNode* AudioContext::createMediaStreamSource(MediaStream* mediaStream, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    if (!mediaStream) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "invalid MediaStream source");
        return 0;
    }

    MediaStreamTrackVector audioTracks = mediaStream->getAudioTracks();
    if (audioTracks.isEmpty()) {
        exceptionState.throwDOMException(
            InvalidStateError,
            "MediaStream has no audio track");
        return 0;
    }

    // Use the first audio track in the media stream.
    MediaStreamTrack* audioTrack = audioTracks[0];
    OwnPtr<AudioSourceProvider> provider = audioTrack->createWebAudioSource();
    MediaStreamAudioSourceNode* node = MediaStreamAudioSourceNode::create(this, mediaStream, audioTrack, provider.release());

    // FIXME: Only stereo streams are supported right now. We should be able to accept multi-channel streams.
    node->setFormat(2, sampleRate());

    refNode(node); // context keeps reference until node is disconnected
    return node;
}

MediaStreamAudioDestinationNode* AudioContext::createMediaStreamDestination()
{
    // Set number of output channels to stereo by default.
    return MediaStreamAudioDestinationNode::create(this, 2);
}

ScriptProcessorNode* AudioContext::createScriptProcessor(ExceptionState& exceptionState)
{
    // Set number of input/output channels to stereo by default.
    return createScriptProcessor(0, 2, 2, exceptionState);
}

ScriptProcessorNode* AudioContext::createScriptProcessor(size_t bufferSize, ExceptionState& exceptionState)
{
    // Set number of input/output channels to stereo by default.
    return createScriptProcessor(bufferSize, 2, 2, exceptionState);
}

ScriptProcessorNode* AudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState& exceptionState)
{
    // Set number of output channels to stereo by default.
    return createScriptProcessor(bufferSize, numberOfInputChannels, 2, exceptionState);
}

ScriptProcessorNode* AudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    ScriptProcessorNode* node = ScriptProcessorNode::create(this, m_destinationNode->sampleRate(), bufferSize, numberOfInputChannels, numberOfOutputChannels);

    if (!node) {
        if (!numberOfInputChannels && !numberOfOutputChannels) {
            exceptionState.throwDOMException(
                IndexSizeError,
                "number of input channels and output channels cannot both be zero.");
        } else if (numberOfInputChannels > AudioContext::maxNumberOfChannels()) {
            exceptionState.throwDOMException(
                IndexSizeError,
                "number of input channels (" + String::number(numberOfInputChannels)
                + ") exceeds maximum ("
                + String::number(AudioContext::maxNumberOfChannels()) + ").");
        } else if (numberOfOutputChannels > AudioContext::maxNumberOfChannels()) {
            exceptionState.throwDOMException(
                IndexSizeError,
                "number of output channels (" + String::number(numberOfInputChannels)
                + ") exceeds maximum ("
                + String::number(AudioContext::maxNumberOfChannels()) + ").");
        } else {
            exceptionState.throwDOMException(
                IndexSizeError,
                "buffer size (" + String::number(bufferSize)
                + ") must be a power of two between 256 and 16384.");
        }
        return 0;
    }

    refNode(node); // context keeps reference until we stop making javascript rendering callbacks
    return node;
}

BiquadFilterNode* AudioContext::createBiquadFilter()
{
    ASSERT(isMainThread());
    return BiquadFilterNode::create(this, m_destinationNode->sampleRate());
}

WaveShaperNode* AudioContext::createWaveShaper()
{
    ASSERT(isMainThread());
    return WaveShaperNode::create(this);
}

PannerNode* AudioContext::createPanner()
{
    ASSERT(isMainThread());
    return PannerNode::create(this, m_destinationNode->sampleRate());
}

ConvolverNode* AudioContext::createConvolver()
{
    ASSERT(isMainThread());
    return ConvolverNode::create(this, m_destinationNode->sampleRate());
}

DynamicsCompressorNode* AudioContext::createDynamicsCompressor()
{
    ASSERT(isMainThread());
    return DynamicsCompressorNode::create(this, m_destinationNode->sampleRate());
}

AnalyserNode* AudioContext::createAnalyser()
{
    ASSERT(isMainThread());
    return AnalyserNode::create(this, m_destinationNode->sampleRate());
}

GainNode* AudioContext::createGain()
{
    ASSERT(isMainThread());
    return GainNode::create(this, m_destinationNode->sampleRate());
}

DelayNode* AudioContext::createDelay(ExceptionState& exceptionState)
{
    const double defaultMaxDelayTime = 1;
    return createDelay(defaultMaxDelayTime, exceptionState);
}

DelayNode* AudioContext::createDelay(double maxDelayTime, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    DelayNode* node = DelayNode::create(this, m_destinationNode->sampleRate(), maxDelayTime, exceptionState);
    if (exceptionState.hadException())
        return 0;
    return node;
}

ChannelSplitterNode* AudioContext::createChannelSplitter(ExceptionState& exceptionState)
{
    const unsigned ChannelSplitterDefaultNumberOfOutputs = 6;
    return createChannelSplitter(ChannelSplitterDefaultNumberOfOutputs, exceptionState);
}

ChannelSplitterNode* AudioContext::createChannelSplitter(size_t numberOfOutputs, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    ChannelSplitterNode* node = ChannelSplitterNode::create(this, m_destinationNode->sampleRate(), numberOfOutputs);

    if (!node) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "number of outputs (" + String::number(numberOfOutputs)
            + ") must be between 1 and "
            + String::number(AudioContext::maxNumberOfChannels()) + ".");
        return 0;
    }

    return node;
}

ChannelMergerNode* AudioContext::createChannelMerger(ExceptionState& exceptionState)
{
    const unsigned ChannelMergerDefaultNumberOfInputs = 6;
    return createChannelMerger(ChannelMergerDefaultNumberOfInputs, exceptionState);
}

ChannelMergerNode* AudioContext::createChannelMerger(size_t numberOfInputs, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    ChannelMergerNode* node = ChannelMergerNode::create(this, m_destinationNode->sampleRate(), numberOfInputs);

    if (!node) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "number of inputs (" + String::number(numberOfInputs)
            + ") must be between 1 and "
            + String::number(AudioContext::maxNumberOfChannels()) + ".");
        return 0;
    }

    return node;
}

OscillatorNode* AudioContext::createOscillator()
{
    ASSERT(isMainThread());

    OscillatorNode* node = OscillatorNode::create(this, m_destinationNode->sampleRate());

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls AudioContext::notifyNodeFinishedProcessing().
    refNode(node);

    return node;
}

PeriodicWave* AudioContext::createPeriodicWave(Float32Array* real, Float32Array* imag, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());

    if (!real) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid real array");
        return 0;
    }

    if (!imag) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid imaginary array");
        return 0;
    }

    if (real->length() != imag->length()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "length of real array (" + String::number(real->length())
            + ") and length of imaginary array (" +  String::number(imag->length())
            + ") must match.");
        return 0;
    }

    if (real->length() > 4096) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "length of real array (" + String::number(real->length())
            + ") exceeds allowed maximum of 4096");
        return 0;
    }

    if (imag->length() > 4096) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "length of imaginary array (" + String::number(imag->length())
            + ") exceeds allowed maximum of 4096");
        return 0;
    }

    return PeriodicWave::create(sampleRate(), real, imag);
}

void AudioContext::notifyNodeFinishedProcessing(AudioNode* node)
{
    ASSERT(isAudioThread());
    m_finishedNodes.append(node);
}

void AudioContext::derefFinishedSourceNodes()
{
    ASSERT(isGraphOwner());
    ASSERT(isAudioThread());
    for (unsigned i = 0; i < m_finishedNodes.size(); i++)
        derefNode(m_finishedNodes[i]);

    m_finishedNodes.clear();
}

void AudioContext::refNode(AudioNode* node)
{
    ASSERT(isMainThread());
    AutoLocker locker(this);

    m_referencedNodes.append(node);
    node->makeConnection();
}

void AudioContext::derefNode(AudioNode* node)
{
    ASSERT(isGraphOwner());

    for (unsigned i = 0; i < m_referencedNodes.size(); ++i) {
        if (node == m_referencedNodes[i].get()) {
            node->breakConnection();
            m_referencedNodes.remove(i);
            break;
        }
    }
}

void AudioContext::derefUnfinishedSourceNodes()
{
    ASSERT(isMainThread());
    for (unsigned i = 0; i < m_referencedNodes.size(); ++i)
        m_referencedNodes[i]->breakConnection();

    m_referencedNodes.clear();
}

void AudioContext::lock(bool& mustReleaseLock)
{
    // Don't allow regular lock in real-time audio thread.
    ASSERT(isMainThread());

    ThreadIdentifier thisThread = currentThread();

    if (thisThread == m_graphOwnerThread) {
        // We already have the lock.
        mustReleaseLock = false;
    } else {
        // Acquire the lock.
        m_contextGraphMutex.lock();
        m_graphOwnerThread = thisThread;
        mustReleaseLock = true;
    }
}

bool AudioContext::tryLock(bool& mustReleaseLock)
{
    ThreadIdentifier thisThread = currentThread();
    bool isAudioThread = thisThread == audioThread();

    // Try to catch cases of using try lock on main thread - it should use regular lock.
    ASSERT(isAudioThread);

    if (!isAudioThread) {
        // In release build treat tryLock() as lock() (since above ASSERT(isAudioThread) never fires) - this is the best we can do.
        lock(mustReleaseLock);
        return true;
    }

    bool hasLock;

    if (thisThread == m_graphOwnerThread) {
        // Thread already has the lock.
        hasLock = true;
        mustReleaseLock = false;
    } else {
        // Don't already have the lock - try to acquire it.
        hasLock = m_contextGraphMutex.tryLock();

        if (hasLock)
            m_graphOwnerThread = thisThread;

        mustReleaseLock = hasLock;
    }

    return hasLock;
}

void AudioContext::unlock()
{
    ASSERT(currentThread() == m_graphOwnerThread);

    m_graphOwnerThread = UndefinedThreadIdentifier;
    m_contextGraphMutex.unlock();
}

bool AudioContext::isAudioThread() const
{
    return currentThread() == m_audioThread;
}

bool AudioContext::isGraphOwner() const
{
    return currentThread() == m_graphOwnerThread;
}

void AudioContext::addDeferredBreakConnection(AudioNode& node)
{
    ASSERT(isAudioThread());
    m_deferredBreakConnectionList.append(&node);
}

void AudioContext::handlePreRenderTasks()
{
    ASSERT(isAudioThread());

    // At the beginning of every render quantum, try to update the internal rendering graph state (from main thread changes).
    // It's OK if the tryLock() fails, we'll just take slightly longer to pick up the changes.
    bool mustReleaseLock;
    if (tryLock(mustReleaseLock)) {
        // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
        handleDirtyAudioSummingJunctions();
        handleDirtyAudioNodeOutputs();

        updateAutomaticPullNodes();

        if (mustReleaseLock)
            unlock();
    }
}

void AudioContext::handlePostRenderTasks()
{
    ASSERT(isAudioThread());

    // Must use a tryLock() here too.  Don't worry, the lock will very rarely be contended and this method is called frequently.
    // The worst that can happen is that there will be some nodes which will take slightly longer than usual to be deleted or removed
    // from the render graph (in which case they'll render silence).
    bool mustReleaseLock;
    if (tryLock(mustReleaseLock)) {
        // Take care of AudioNode tasks where the tryLock() failed previously.
        handleDeferredAudioNodeTasks();

        // Dynamically clean up nodes which are no longer needed.
        derefFinishedSourceNodes();

        // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
        handleDirtyAudioSummingJunctions();
        handleDirtyAudioNodeOutputs();

        updateAutomaticPullNodes();

        if (mustReleaseLock)
            unlock();
    }
}

void AudioContext::handleDeferredAudioNodeTasks()
{
    ASSERT(isAudioThread() && isGraphOwner());

    for (unsigned i = 0; i < m_deferredBreakConnectionList.size(); ++i)
        m_deferredBreakConnectionList[i]->breakConnectionWithLock();
    m_deferredBreakConnectionList.clear();
}

void AudioContext::registerLiveNode(AudioNode& node)
{
    ASSERT(isMainThread());
    m_liveNodes.add(&node, adoptPtr(new AudioNodeDisposer(node)));
}

AudioContext::AudioNodeDisposer::~AudioNodeDisposer()
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(m_node.context());
    m_node.dispose();
}

void AudioContext::registerLiveAudioSummingJunction(AudioSummingJunction& junction)
{
    ASSERT(isMainThread());
    m_liveAudioSummingJunctions.add(&junction, adoptPtr(new AudioSummingJunctionDisposer(junction)));
}

AudioContext::AudioSummingJunctionDisposer::~AudioSummingJunctionDisposer()
{
    ASSERT(isMainThread());
    m_junction.dispose();
}

void AudioContext::unmarkDirtyNode(AudioNode& node)
{
    ASSERT(isGraphOwner());

    // Before deleting the node, clear out any AudioNodeOutputs from
    // m_dirtyAudioNodeOutputs.
    unsigned numberOfOutputs = node.numberOfOutputs();
    for (unsigned i = 0; i < numberOfOutputs; ++i)
        m_dirtyAudioNodeOutputs.remove(node.output(i));
}

void AudioContext::markSummingJunctionDirty(AudioSummingJunction* summingJunction)
{
    ASSERT(isGraphOwner());
    m_dirtySummingJunctions.add(summingJunction);
}

void AudioContext::removeMarkedSummingJunction(AudioSummingJunction* summingJunction)
{
    ASSERT(isMainThread());
    AutoLocker locker(this);
    m_dirtySummingJunctions.remove(summingJunction);
}

void AudioContext::markAudioNodeOutputDirty(AudioNodeOutput* output)
{
    ASSERT(isGraphOwner());
    ASSERT(isMainThread());
    m_dirtyAudioNodeOutputs.add(output);
}

void AudioContext::handleDirtyAudioSummingJunctions()
{
    ASSERT(isGraphOwner());

    for (HashSet<AudioSummingJunction*>::iterator i = m_dirtySummingJunctions.begin(); i != m_dirtySummingJunctions.end(); ++i)
        (*i)->updateRenderingState();

    m_dirtySummingJunctions.clear();
}

void AudioContext::handleDirtyAudioNodeOutputs()
{
    ASSERT(isGraphOwner());

    for (HashSet<AudioNodeOutput*>::iterator i = m_dirtyAudioNodeOutputs.begin(); i != m_dirtyAudioNodeOutputs.end(); ++i)
        (*i)->updateRenderingState();

    m_dirtyAudioNodeOutputs.clear();
}

void AudioContext::addAutomaticPullNode(AudioNode* node)
{
    ASSERT(isGraphOwner());

    if (!m_automaticPullNodes.contains(node)) {
        m_automaticPullNodes.add(node);
        m_automaticPullNodesNeedUpdating = true;
    }
}

void AudioContext::removeAutomaticPullNode(AudioNode* node)
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodes.contains(node)) {
        m_automaticPullNodes.remove(node);
        m_automaticPullNodesNeedUpdating = true;
    }
}

void AudioContext::updateAutomaticPullNodes()
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodesNeedUpdating) {
        // Copy from m_automaticPullNodes to m_renderingAutomaticPullNodes.
        m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());

        unsigned j = 0;
        for (HashSet<AudioNode*>::iterator i = m_automaticPullNodes.begin(); i != m_automaticPullNodes.end(); ++i, ++j) {
            AudioNode* output = *i;
            m_renderingAutomaticPullNodes[j] = output;
        }

        m_automaticPullNodesNeedUpdating = false;
    }
}

void AudioContext::processAutomaticPullNodes(size_t framesToProcess)
{
    ASSERT(isAudioThread());

    for (unsigned i = 0; i < m_renderingAutomaticPullNodes.size(); ++i)
        m_renderingAutomaticPullNodes[i]->processIfNecessary(framesToProcess);
}

const AtomicString& AudioContext::interfaceName() const
{
    return EventTargetNames::AudioContext;
}

ExecutionContext* AudioContext::executionContext() const
{
    return m_isStopScheduled ? 0 : ActiveDOMObject::executionContext();
}

void AudioContext::startRendering()
{
    destination()->startRendering();
}

void AudioContext::fireCompletionEvent()
{
    ASSERT(isMainThread());
    if (!isMainThread())
        return;

    AudioBuffer* renderedBuffer = m_renderTarget.get();

    ASSERT(renderedBuffer);
    if (!renderedBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (executionContext()) {
        // Call the offline rendering completion event listener.
        dispatchEvent(OfflineAudioCompletionEvent::create(renderedBuffer));
    }
}

void AudioContext::trace(Visitor* visitor)
{
    visitor->trace(m_renderTarget);
    visitor->trace(m_destinationNode);
    visitor->trace(m_listener);
    visitor->trace(m_referencedNodes);
    visitor->trace(m_liveNodes);
    visitor->trace(m_liveAudioSummingJunctions);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
