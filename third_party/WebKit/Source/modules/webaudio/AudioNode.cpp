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

#include "modules/webaudio/AudioNode.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/AudioParam.h"
#include "wtf/Atomics.h"
#include "wtf/MainThread.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

unsigned AudioNode::s_instanceCount = 0;

AudioNode::AudioNode(AudioContext* context, float sampleRate)
    : m_isInitialized(false)
    , m_nodeType(NodeTypeUnknown)
    , m_context(context)
    , m_sampleRate(sampleRate)
    , m_lastProcessingTime(-1)
    , m_lastNonSilentTime(-1)
#if !ENABLE(OILPAN)
    , m_normalRefCount(1) // start out with normal refCount == 1 (like WTF::RefCounted class)
#endif
    , m_connectionRefCount(0)
#if !ENABLE(OILPAN)
    , m_isMarkedForDeletion(false)
#endif
    , m_isDisabled(false)
#if ENABLE(ASSERT)
    , m_didCallDispose(false)
#endif
    , m_channelCount(2)
    , m_channelCountMode(Max)
    , m_channelInterpretation(AudioBus::Speakers)
{
    ScriptWrappable::init(this);
#if ENABLE(OILPAN)
    m_context->registerLiveNode(*this);
#endif
#if DEBUG_AUDIONODE_REFERENCES
    if (!s_isNodeCountInitialized) {
        s_isNodeCountInitialized = true;
        atexit(AudioNode::printNodeCounts);
    }
#endif
    ++s_instanceCount;
}

AudioNode::~AudioNode()
{
    ASSERT(m_didCallDispose);
    --s_instanceCount;
#if DEBUG_AUDIONODE_REFERENCES
    --s_nodeCount[nodeType()];
#if ENABLE(OILPAN)
    fprintf(stderr, "%p: %d: AudioNode::~AudioNode() %d\n", this, nodeType(), m_connectionRefCount);
#else
    fprintf(stderr, "%p: %d: AudioNode::~AudioNode() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif
#endif
}

void AudioNode::initialize()
{
    m_isInitialized = true;
}

void AudioNode::uninitialize()
{
    m_isInitialized = false;
}

void AudioNode::dispose()
{
    ASSERT(isMainThread());
    ASSERT(context()->isGraphOwner());

    context()->removeAutomaticPullNode(this);
    for (unsigned i = 0; i < m_outputs.size(); ++i)
        output(i)->disconnectAll();
    context()->unmarkDirtyNode(*this);
#if ENABLE(ASSERT)
    m_didCallDispose = true;
#endif
}

String AudioNode::nodeTypeName() const
{
    switch (m_nodeType) {
    case NodeTypeDestination:
        return "AudioDestinationNode";
    case NodeTypeOscillator:
        return "OscillatorNode";
    case NodeTypeAudioBufferSource:
        return "AudioBufferSourceNode";
    case NodeTypeMediaElementAudioSource:
        return "MediaElementAudioSourceNode";
    case NodeTypeMediaStreamAudioDestination:
        return "MediaStreamAudioDestinationNode";
    case NodeTypeMediaStreamAudioSource:
        return "MediaStreamAudioSourceNode";
    case NodeTypeJavaScript:
        return "ScriptProcessorNode";
    case NodeTypeBiquadFilter:
        return "BiquadFilterNode";
    case NodeTypePanner:
        return "PannerNode";
    case NodeTypeConvolver:
        return "ConvolverNode";
    case NodeTypeDelay:
        return "DelayNode";
    case NodeTypeGain:
        return "GainNode";
    case NodeTypeChannelSplitter:
        return "ChannelSplitterNode";
    case NodeTypeChannelMerger:
        return "ChannelMergerNode";
    case NodeTypeAnalyser:
        return "AnalyserNode";
    case NodeTypeDynamicsCompressor:
        return "DynamicsCompressorNode";
    case NodeTypeWaveShaper:
        return "WaveShaperNode";
    case NodeTypeUnknown:
    case NodeTypeEnd:
    default:
        ASSERT_NOT_REACHED();
        return "UnknownNode";
    }
}

void AudioNode::setNodeType(NodeType type)
{
    m_nodeType = type;

#if DEBUG_AUDIONODE_REFERENCES
    ++s_nodeCount[type];
#endif
}

void AudioNode::addInput()
{
    m_inputs.append(AudioNodeInput::create(*this));
}

void AudioNode::addOutput(PassOwnPtrWillBeRawPtr<AudioNodeOutput> output)
{
    m_outputs.append(output);
}

AudioNodeInput* AudioNode::input(unsigned i)
{
    if (i < m_inputs.size())
        return m_inputs[i].get();
    return 0;
}

AudioNodeOutput* AudioNode::output(unsigned i)
{
    if (i < m_outputs.size())
        return m_outputs[i].get();
    return 0;
}

void AudioNode::connect(AudioNode* destination, unsigned outputIndex, unsigned inputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (!destination) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid destination node.");
        return;
    }

    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "output index (" + String::number(outputIndex) + ") exceeds number of outputs (" + String::number(numberOfOutputs()) + ").");
        return;
    }

    if (destination && inputIndex >= destination->numberOfInputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "input index (" + String::number(inputIndex) + ") exceeds number of inputs (" + String::number(destination->numberOfInputs()) + ").");
        return;
    }

    if (context() != destination->context()) {
        exceptionState.throwDOMException(
            SyntaxError,
            "cannot connect to a destination belonging to a different audio context.");
        return;
    }

    AudioNodeInput* input = destination->input(inputIndex);
    input->connect(*output(outputIndex));

    // Let context know that a connection has been made.
    context()->incrementConnectionCount();
}

void AudioNode::connect(AudioParam* param, unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (!param) {
        exceptionState.throwDOMException(
            SyntaxError,
            "invalid AudioParam.");
        return;
    }

    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "output index (" + String::number(outputIndex) + ") exceeds number of outputs (" + String::number(numberOfOutputs()) + ").");
        return;
    }

    if (context() != param->context()) {
        exceptionState.throwDOMException(
            SyntaxError,
            "cannot connect to an AudioParam belonging to a different audio context.");
        return;
    }

    param->connect(*output(outputIndex));
}

void AudioNode::disconnect(unsigned outputIndex, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs()) {
        exceptionState.throwDOMException(
            IndexSizeError,
            "output index (" + String::number(outputIndex) + ") exceeds number of outputs (" + String::number(numberOfOutputs()) + ").");
        return;
    }

    AudioNodeOutput* output = this->output(outputIndex);
    output->disconnectAll();
}

unsigned long AudioNode::channelCount()
{
    return m_channelCount;
}

void AudioNode::setChannelCount(unsigned long channelCount, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (channelCount > 0 && channelCount <= AudioContext::maxNumberOfChannels()) {
        if (m_channelCount != channelCount) {
            m_channelCount = channelCount;
            if (m_channelCountMode != Max)
                updateChannelsForInputs();
        }
    } else {
        exceptionState.throwDOMException(
            NotSupportedError,
            "channel count (" + String::number(channelCount) + ") must be between 1 and " + String::number(AudioContext::maxNumberOfChannels()) + ".");
    }
}

String AudioNode::channelCountMode()
{
    switch (m_channelCountMode) {
    case Max:
        return "max";
    case ClampedMax:
        return "clamped-max";
    case Explicit:
        return "explicit";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void AudioNode::setChannelCountMode(const String& mode, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    ChannelCountMode oldMode = m_channelCountMode;

    if (mode == "max") {
        m_channelCountMode = Max;
    } else if (mode == "clamped-max") {
        m_channelCountMode = ClampedMax;
    } else if (mode == "explicit") {
        m_channelCountMode = Explicit;
    } else {
        ASSERT_NOT_REACHED();
    }

    if (m_channelCountMode != oldMode)
        updateChannelsForInputs();
}

String AudioNode::channelInterpretation()
{
    switch (m_channelInterpretation) {
    case AudioBus::Speakers:
        return "speakers";
    case AudioBus::Discrete:
        return "discrete";
    }
    ASSERT_NOT_REACHED();
    return "";
}

void AudioNode::setChannelInterpretation(const String& interpretation, ExceptionState& exceptionState)
{
    ASSERT(isMainThread());
    AudioContext::AutoLocker locker(context());

    if (interpretation == "speakers") {
        m_channelInterpretation = AudioBus::Speakers;
    } else if (interpretation == "discrete") {
        m_channelInterpretation = AudioBus::Discrete;
    } else {
        ASSERT_NOT_REACHED();
    }
}

void AudioNode::updateChannelsForInputs()
{
    for (unsigned i = 0; i < m_inputs.size(); ++i)
        input(i)->changedOutputs();
}

const AtomicString& AudioNode::interfaceName() const
{
    return EventTargetNames::AudioNode;
}

ExecutionContext* AudioNode::executionContext() const
{
    return const_cast<AudioNode*>(this)->context()->executionContext();
}

void AudioNode::processIfNecessary(size_t framesToProcess)
{
    ASSERT(context()->isAudioThread());

    if (!isInitialized())
        return;

    // Ensure that we only process once per rendering quantum.
    // This handles the "fanout" problem where an output is connected to multiple inputs.
    // The first time we're called during this time slice we process, but after that we don't want to re-process,
    // instead our output(s) will already have the results cached in their bus;
    double currentTime = context()->currentTime();
    if (m_lastProcessingTime != currentTime) {
        m_lastProcessingTime = currentTime; // important to first update this time because of feedback loops in the rendering graph

        pullInputs(framesToProcess);

        bool silentInputs = inputsAreSilent();
        if (!silentInputs)
            m_lastNonSilentTime = (context()->currentSampleFrame() + framesToProcess) / static_cast<double>(m_sampleRate);

        if (silentInputs && propagatesSilence())
            silenceOutputs();
        else {
            process(framesToProcess);
            unsilenceOutputs();
        }
    }
}

void AudioNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context()->isAudioThread() && context()->isGraphOwner());

    ASSERT(m_inputs.contains(input));
    if (!m_inputs.contains(input))
        return;

    input->updateInternalBus();
}

bool AudioNode::propagatesSilence() const
{
    return m_lastNonSilentTime + latencyTime() + tailTime() < context()->currentTime();
}

void AudioNode::pullInputs(size_t framesToProcess)
{
    ASSERT(context()->isAudioThread());

    // Process all of the AudioNodes connected to our inputs.
    for (unsigned i = 0; i < m_inputs.size(); ++i)
        input(i)->pull(0, framesToProcess);
}

bool AudioNode::inputsAreSilent()
{
    for (unsigned i = 0; i < m_inputs.size(); ++i) {
        if (!input(i)->bus()->isSilent())
            return false;
    }
    return true;
}

void AudioNode::silenceOutputs()
{
    for (unsigned i = 0; i < m_outputs.size(); ++i)
        output(i)->bus()->zero();
}

void AudioNode::unsilenceOutputs()
{
    for (unsigned i = 0; i < m_outputs.size(); ++i)
        output(i)->bus()->clearSilentFlag();
}

void AudioNode::enableOutputsIfNecessary()
{
    if (m_isDisabled && m_connectionRefCount > 0) {
        ASSERT(isMainThread());
        AudioContext::AutoLocker locker(context());

        m_isDisabled = false;
        for (unsigned i = 0; i < m_outputs.size(); ++i)
            output(i)->enable();
    }
}

void AudioNode::disableOutputsIfNecessary()
{
    // Disable outputs if appropriate. We do this if the number of connections is 0 or 1. The case
    // of 0 is from finishDeref() where there are no connections left. The case of 1 is from
    // AudioNodeInput::disable() where we want to disable outputs when there's only one connection
    // left because we're ready to go away, but can't quite yet.
    if (m_connectionRefCount <= 1 && !m_isDisabled) {
        // Still may have JavaScript references, but no more "active" connection references, so put all of our outputs in a "dormant" disabled state.
        // Garbage collection may take a very long time after this time, so the "dormant" disabled nodes should not bog down the rendering...

        // As far as JavaScript is concerned, our outputs must still appear to be connected.
        // But internally our outputs should be disabled from the inputs they're connected to.
        // disable() can recursively deref connections (and call disable()) down a whole chain of connected nodes.

        // FIXME: we special case the convolver and delay since they have a significant tail-time and shouldn't be disconnected simply
        // because they no longer have any input connections. This needs to be handled more generally where AudioNodes have
        // a tailTime attribute. Then the AudioNode only needs to remain "active" for tailTime seconds after there are no
        // longer any active connections.
        if (nodeType() != NodeTypeConvolver && nodeType() != NodeTypeDelay) {
            m_isDisabled = true;
            for (unsigned i = 0; i < m_outputs.size(); ++i)
                output(i)->disable();
        }
    }
}

#if !ENABLE(OILPAN)
void AudioNode::ref()
{
    atomicIncrement(&m_normalRefCount);

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::ref() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif
}
#endif

void AudioNode::makeConnection()
{
    atomicIncrement(&m_connectionRefCount);
    // See the disabling code in finishDeref() below. This handles the case
    // where a node is being re-connected after being used at least once and
    // disconnected. In this case, we need to re-enable.
    enableOutputsIfNecessary();
}

#if !ENABLE(OILPAN)
void AudioNode::deref()
{
    // The actual work for deref happens completely within the audio context's
    // graph lock. In the case of the audio thread, we must use a tryLock to
    // avoid glitches.
    bool hasLock = false;
    bool mustReleaseLock = false;

    if (context()->isAudioThread()) {
        // Real-time audio thread must not contend lock (to avoid glitches).
        hasLock = context()->tryLock(mustReleaseLock);
    } else {
        context()->lock(mustReleaseLock);
        hasLock = true;
    }

    if (hasLock) {
        // This is where the real deref work happens.
        finishDeref();

        if (mustReleaseLock)
            context()->unlock();
    } else {
        // We were unable to get the lock, so put this in a list to finish up later.
        ASSERT(context()->isAudioThread());
        context()->addDeferredFinishDeref(this);
    }

    // Once AudioContext::uninitialize() is called there's no more chances for deleteMarkedNodes() to get called, so we call here.
    // We can't call in AudioContext::~AudioContext() since it will never be called as long as any AudioNode is alive
    // because AudioNodes keep a reference to the context.
    if (!context()->isInitialized())
        context()->deleteMarkedNodes();
}
#endif

void AudioNode::breakConnection()
{
    // The actual work for deref happens completely within the audio context's
    // graph lock. In the case of the audio thread, we must use a tryLock to
    // avoid glitches.
    bool hasLock = false;
    bool mustReleaseLock = false;

    if (context()->isAudioThread()) {
        // Real-time audio thread must not contend lock (to avoid glitches).
        hasLock = context()->tryLock(mustReleaseLock);
    } else {
        context()->lock(mustReleaseLock);
        hasLock = true;
    }

    if (hasLock) {
        breakConnectionWithLock();

        if (mustReleaseLock)
            context()->unlock();
    } else {
        // We were unable to get the lock, so put this in a list to finish up
        // later.
        ASSERT(context()->isAudioThread());
        context()->addDeferredBreakConnection(*this);
    }
}

void AudioNode::breakConnectionWithLock()
{
    atomicDecrement(&m_connectionRefCount);
#if !ENABLE(OILPAN)
    ASSERT(m_normalRefCount > 0);
#endif
    if (!m_connectionRefCount)
        disableOutputsIfNecessary();
}

#if !ENABLE(OILPAN)
void AudioNode::finishDeref()
{
    ASSERT(context()->isGraphOwner());

    ASSERT(m_normalRefCount > 0);
    atomicDecrement(&m_normalRefCount);

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::deref() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif

    if (!m_normalRefCount && !m_isMarkedForDeletion) {
        // Mark for deletion at end of each render quantum or when context shuts
        // down.
        context()->markForDeletion(this);
        m_isMarkedForDeletion = true;
    }
}
#endif

#if DEBUG_AUDIONODE_REFERENCES

bool AudioNode::s_isNodeCountInitialized = false;
int AudioNode::s_nodeCount[NodeTypeEnd];

void AudioNode::printNodeCounts()
{
    fprintf(stderr, "\n\n");
    fprintf(stderr, "===========================\n");
    fprintf(stderr, "AudioNode: reference counts\n");
    fprintf(stderr, "===========================\n");

    for (unsigned i = 0; i < NodeTypeEnd; ++i)
        fprintf(stderr, "%d: %d\n", i, s_nodeCount[i]);

    fprintf(stderr, "===========================\n\n\n");
}

#endif // DEBUG_AUDIONODE_REFERENCES

void AudioNode::trace(Visitor* visitor)
{
    visitor->trace(m_context);
    visitor->trace(m_inputs);
    visitor->trace(m_outputs);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
