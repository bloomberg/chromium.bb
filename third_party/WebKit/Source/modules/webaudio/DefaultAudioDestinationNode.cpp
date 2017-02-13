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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/DefaultAudioDestinationNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/BaseAudioContext.h"

namespace blink {

DefaultAudioDestinationHandler::DefaultAudioDestinationHandler(
    AudioNode& node,
    const WebAudioLatencyHint& latencyHint)
    : AudioDestinationHandler(node),
      m_numberOfInputChannels(0),
      m_latencyHint(latencyHint) {
  // Node-specific default mixing rules.
  m_channelCount = 2;
  setInternalChannelCountMode(Explicit);
  setInternalChannelInterpretation(AudioBus::Speakers);
}

PassRefPtr<DefaultAudioDestinationHandler>
DefaultAudioDestinationHandler::create(AudioNode& node,
                                       const WebAudioLatencyHint& latencyHint) {
  return adoptRef(new DefaultAudioDestinationHandler(node, latencyHint));
}

DefaultAudioDestinationHandler::~DefaultAudioDestinationHandler() {
  DCHECK(!isInitialized());
}

void DefaultAudioDestinationHandler::dispose() {
  uninitialize();
  AudioDestinationHandler::dispose();
}

void DefaultAudioDestinationHandler::initialize() {
  DCHECK(isMainThread());
  if (isInitialized())
    return;

  createDestination();
  AudioHandler::initialize();
}

void DefaultAudioDestinationHandler::uninitialize() {
  DCHECK(isMainThread());
  if (!isInitialized())
    return;

  m_destination->stop();
  m_numberOfInputChannels = 0;

  AudioHandler::uninitialize();
}

void DefaultAudioDestinationHandler::createDestination() {
  m_destination = AudioDestination::create(*this, channelCount(), m_latencyHint,
                                           context()->getSecurityOrigin());
}

void DefaultAudioDestinationHandler::startRendering() {
  DCHECK(isInitialized());
  if (isInitialized()) {
    DCHECK(!m_destination->isPlaying());
    m_destination->start();
  }
}

void DefaultAudioDestinationHandler::stopRendering() {
  DCHECK(isInitialized());
  if (isInitialized()) {
    DCHECK(m_destination->isPlaying());
    m_destination->stop();
  }
}

unsigned long DefaultAudioDestinationHandler::maxChannelCount() const {
  return AudioDestination::maxChannelCount();
}

size_t DefaultAudioDestinationHandler::callbackBufferSize() const {
  return m_destination->callbackBufferSize();
}

void DefaultAudioDestinationHandler::setChannelCount(
    unsigned long channelCount,
    ExceptionState& exceptionState) {
  // The channelCount for the input to this node controls the actual number of
  // channels we send to the audio hardware. It can only be set depending on the
  // maximum number of channels supported by the hardware.

  DCHECK(isMainThread());

  if (!maxChannelCount() || channelCount > maxChannelCount()) {
    exceptionState.throwDOMException(
        IndexSizeError,
        ExceptionMessages::indexOutsideRange<unsigned>(
            "channel count", channelCount, 1, ExceptionMessages::InclusiveBound,
            maxChannelCount(), ExceptionMessages::InclusiveBound));
    return;
  }

  unsigned long oldChannelCount = this->channelCount();
  AudioHandler::setChannelCount(channelCount, exceptionState);

  if (!exceptionState.hadException() &&
      this->channelCount() != oldChannelCount && isInitialized()) {
    // Re-create destination.
    m_destination->stop();
    createDestination();
    m_destination->start();
  }
}

double DefaultAudioDestinationHandler::sampleRate() const {
  return m_destination ? m_destination->sampleRate() : 0;
}

int DefaultAudioDestinationHandler::framesPerBuffer() const {
  return m_destination ? m_destination->framesPerBuffer() : 0;
}

// ----------------------------------------------------------------

DefaultAudioDestinationNode::DefaultAudioDestinationNode(
    BaseAudioContext& context,
    const WebAudioLatencyHint& latencyHint)
    : AudioDestinationNode(context) {
  setHandler(DefaultAudioDestinationHandler::create(*this, latencyHint));
}

DefaultAudioDestinationNode* DefaultAudioDestinationNode::create(
    BaseAudioContext* context,
    const WebAudioLatencyHint& latencyHint) {
  return new DefaultAudioDestinationNode(*context, latencyHint);
}

}  // namespace blink
