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

#include "modules/webaudio/ChannelSplitterNode.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/ChannelSplitterOptions.h"

namespace blink {

ChannelSplitterHandler::ChannelSplitterHandler(AudioNode& node,
                                               float sample_rate,
                                               unsigned number_of_outputs)
    : AudioHandler(kNodeTypeChannelSplitter, node, sample_rate) {
  // These properties are fixed and cannot be changed by the user.
  channel_count_ = number_of_outputs;
  SetInternalChannelCountMode(kExplicit);
  AddInput();

  // Create a fixed number of outputs (able to handle the maximum number of
  // channels fed to an input).
  for (unsigned i = 0; i < number_of_outputs; ++i)
    AddOutput(1);

  Initialize();
}

RefPtr<ChannelSplitterHandler> ChannelSplitterHandler::Create(
    AudioNode& node,
    float sample_rate,
    unsigned number_of_outputs) {
  return WTF::AdoptRef(
      new ChannelSplitterHandler(node, sample_rate, number_of_outputs));
}

void ChannelSplitterHandler::Process(size_t frames_to_process) {
  AudioBus* source = Input(0).Bus();
  DCHECK(source);
  DCHECK_EQ(frames_to_process, source->length());

  unsigned number_of_source_channels = source->NumberOfChannels();

  for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
    AudioBus* destination = Output(i).Bus();
    DCHECK(destination);

    if (i < number_of_source_channels) {
      // Split the channel out if it exists in the source.
      // It would be nice to avoid the copy and simply pass along pointers, but
      // this becomes extremely difficult with fanout and fanin.
      destination->Channel(0)->CopyFrom(source->Channel(i));
    } else if (Output(i).RenderingFanOutCount() > 0) {
      // Only bother zeroing out the destination if it's connected to anything
      destination->Zero();
    }
  }
}

void ChannelSplitterHandler::SetChannelCount(unsigned long channel_count,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channelCount cannot be changed from the number of outputs.
  if (channel_count != NumberOfOutputs()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "ChannelSplitter: channelCount cannot be changed from " +
            String::Number(NumberOfOutputs()));
  }
}

void ChannelSplitterHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // channcelCountMode must be 'explicit'.
  if (mode != "explicit") {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "ChannelSplitter: channelCountMode cannot be changed from 'explicit'");
  }
}

// ----------------------------------------------------------------

ChannelSplitterNode::ChannelSplitterNode(BaseAudioContext& context,
                                         unsigned number_of_outputs)
    : AudioNode(context) {
  SetHandler(ChannelSplitterHandler::Create(*this, context.sampleRate(),
                                            number_of_outputs));
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default number of outputs for the splitter node is 6.
  return Create(context, 6, exception_state);
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext& context,
    unsigned number_of_outputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  if (!number_of_outputs ||
      number_of_outputs > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        kIndexSizeError, ExceptionMessages::IndexOutsideRange<size_t>(
                             "number of outputs", number_of_outputs, 1,
                             ExceptionMessages::kInclusiveBound,
                             BaseAudioContext::MaxNumberOfChannels(),
                             ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  return new ChannelSplitterNode(context, number_of_outputs);
}

ChannelSplitterNode* ChannelSplitterNode::Create(
    BaseAudioContext* context,
    const ChannelSplitterOptions& options,
    ExceptionState& exception_state) {
  ChannelSplitterNode* node =
      Create(*context, options.numberOfOutputs(), exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  return node;
}

}  // namespace blink
