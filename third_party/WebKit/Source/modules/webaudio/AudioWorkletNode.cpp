// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletNode.h"

#include "core/dom/TaskRunnerHelper.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/AudioParamDescriptor.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"
#include "modules/webaudio/AudioWorkletProcessor.h"
#include "modules/webaudio/AudioWorkletProcessorDefinition.h"
#include "modules/webaudio/CrossThreadAudioWorkletProcessorInfo.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/heap/Persistent.h"

namespace blink {

AudioWorkletHandler::AudioWorkletHandler(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions& options)
    : AudioHandler(kNodeTypeAudioWorklet, node, sample_rate),
      name_(name),
      param_handler_map_(param_handler_map) {
  DCHECK(IsMainThread());

  for (const auto& param_name : param_handler_map_.Keys()) {
    param_value_map_.Set(
        param_name,
        std::make_unique<AudioFloatArray>(
            AudioUtilities::kRenderQuantumFrames));
  }

  for (unsigned i = 0; i < options.numberOfInputs(); ++i) {
    AddInput();
  }

  // If |options.outputChannelCount| unspecified, all outputs are mono.
  for (unsigned i = 0; i < options.numberOfOutputs(); ++i) {
    unsigned long channel_count = options.hasOutputChannelCount()
        ? options.outputChannelCount()[i]
        : 1;
    AddOutput(channel_count);
  }

  Initialize();
}

AudioWorkletHandler::~AudioWorkletHandler() {
  Uninitialize();
}

scoped_refptr<AudioWorkletHandler> AudioWorkletHandler::Create(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions& options) {
  return WTF::AdoptRef(new AudioWorkletHandler(
      node, sample_rate, name, param_handler_map, options));
}

void AudioWorkletHandler::Process(size_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  // The initialization of handler or the associated processor might not be
  // ready yet. If so, zero out the connected output and exit early.
  if (!processor_) {
    for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
      if (Output(i).IsConnected())
        Output(i).Bus()->Zero();
    }
    return;
  }

  Vector<AudioBus*> inputBuses;
  Vector<AudioBus*> outputBuses;
  for (unsigned i = 0; i < NumberOfInputs(); ++i)
    inputBuses.push_back(Input(i).Bus());
  for (unsigned i = 0; i < NumberOfOutputs(); ++i)
    outputBuses.push_back(Output(i).Bus());

  for (const auto& param_name : param_value_map_.Keys()) {
    const auto param_handler = param_handler_map_.at(param_name);
    AudioFloatArray* param_values = param_value_map_.at(param_name);
    if (param_handler->HasSampleAccurateValues()) {
      param_handler->CalculateSampleAccurateValues(
          param_values->Data(), frames_to_process);
    } else {
      std::fill(param_values->Data(),
                param_values->Data() + frames_to_process,
                param_handler->Value());
    }
  }

  bool process_result = processor_->Process(
      &inputBuses, &outputBuses, &param_value_map_, Context()->currentTime());

  if (!process_result) {
    FinishProcessorOnRenderThread();
  }
}

void AudioWorkletHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  DCHECK(Context()->IsGraphOwner());
  DCHECK(input);

  // Dynamic channel count only works when the node has 1 input and 1 output.
  // Otherwise the channel count(s) should not be dynamically changed.
  if (NumberOfInputs() == 1 && NumberOfOutputs() == 1) {
    DCHECK_EQ(input, &this->Input(0));
    unsigned number_of_input_channels = Input(0).NumberOfChannels();
    if (number_of_input_channels != Output(0).NumberOfChannels()) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(number_of_input_channels);
    }
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

double AudioWorkletHandler::TailTime() const {
  DCHECK(Context()->IsAudioThread());
  return tail_time_;
}

void AudioWorkletHandler::SetProcessorOnRenderThread(
    AudioWorkletProcessor* processor) {
  // TODO(hongchan): unify the thread ID check. The thread ID for this call
  // is different from |Context()->IsAudiothread()|.
  DCHECK(!IsMainThread());
  processor_ = processor;
}

void AudioWorkletHandler::FinishProcessorOnRenderThread() {
  DCHECK(Context()->IsAudioThread());
  // TODO(hongchan): After this point, The handler has no more pending activity
  // and ready for GC.
  Context()->NotifySourceNodeFinishedProcessing(this);
  processor_.Clear();
  tail_time_ = 0;
}

// ----------------------------------------------------------------

AudioWorkletNode::AudioWorkletNode(
    BaseAudioContext& context,
    const String& name,
    const AudioWorkletNodeOptions& options,
    const Vector<CrossThreadAudioParamInfo> param_info_list)
    : AudioNode(context) {
  HeapHashMap<String, Member<AudioParam>> audio_param_map;
  HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map;
  for (const auto& param_info : param_info_list) {
    String param_name = param_info.Name().IsolatedCopy();
    AudioParam* audio_param = AudioParam::Create(context,
                                                 kParamTypeAudioWorklet,
                                                 param_info.DefaultValue(),
                                                 param_info.MinValue(),
                                                 param_info.MaxValue());
    audio_param_map.Set(param_name, audio_param);
    param_handler_map.Set(param_name, WrapRefPtr(&audio_param->Handler()));

    if (options.hasParameterData()) {
      for (const auto& key_value_pair : options.parameterData()) {
        if (key_value_pair.first == param_name)
          audio_param->setInitialValue(key_value_pair.second);
      }
    }
  }
  parameter_map_ = new AudioParamMap(audio_param_map);

  SetHandler(AudioWorkletHandler::Create(*this,
                                         context.sampleRate(),
                                         name,
                                         param_handler_map,
                                         options));
}

AudioWorkletNode* AudioWorkletNode::Create(
    BaseAudioContext* context,
    const String& name,
    const AudioWorkletNodeOptions& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context->IsContextClosed()) {
    context->ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  if (options.numberOfInputs() == 0 && options.numberOfOutputs() == 0) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "AudioWorkletNode cannot be created: Number of inputs and number of "
            "outputs cannot be both zero.");
    return nullptr;
  }

  if (options.hasOutputChannelCount()) {
    if (options.numberOfOutputs() != options.outputChannelCount().size()) {
      exception_state.ThrowDOMException(
          kIndexSizeError,
          "AudioWorkletNode cannot be created: Length of specified "
              "'outputChannelCount' (" +
              String::Number(options.outputChannelCount().size()) +
              ") does not match the given number of outputs (" +
              String::Number(options.numberOfOutputs()) + ").");
      return nullptr;
    }

    for (const auto& channel_count : options.outputChannelCount()) {
      if (channel_count < 1 ||
          channel_count > BaseAudioContext::MaxNumberOfChannels()) {
        exception_state.ThrowDOMException(
            kNotSupportedError,
            ExceptionMessages::IndexOutsideRange<unsigned long>(
                "channel count", channel_count,
                1,
                ExceptionMessages::kInclusiveBound,
                BaseAudioContext::MaxNumberOfChannels(),
                ExceptionMessages::kInclusiveBound));
        return nullptr;
      }
    }
  }

  if (!context->HasWorkletMessagingProxy()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "AudioWorkletNode cannot be created: AudioWorklet does not have a "
            "valid AudioWorkletGlobalScope. Load a script via "
            "audioWorklet.addModule() first.");
    return nullptr;
  }

  AudioWorkletMessagingProxy* proxy = context->WorkletMessagingProxy();

  if (!proxy->IsProcessorRegistered(name)) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "AudioWorkletNode cannot be created: The node name '" + name +
            "' is not defined in AudioWorkletGlobalScope.");
    return nullptr;
  }

  AudioWorkletNode* node = new AudioWorkletNode(
      *context, name, options, proxy->GetParamInfoListForProcessor(name));

  if (!node) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "AudioWorkletNode cannot be created.");
    return nullptr;
  }

  node->HandleChannelOptions(options, exception_state);

  // context keeps reference as a source node.
  context->NotifySourceNodeStartedProcessing(node);

  // This is non-blocking async call. |node| still can be returned to user
  // before the scheduled async task is completed.
  proxy->CreateProcessor(&node->GetWorkletHandler());

  return node;
}

bool AudioWorkletNode::HasPendingActivity() const {
  return !context()->IsContextClosed();
}

AudioParamMap* AudioWorkletNode::parameters() const {
  return parameter_map_;
}

AudioWorkletHandler& AudioWorkletNode::GetWorkletHandler() const {
  return static_cast<AudioWorkletHandler&>(Handler());
}

void AudioWorkletNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(parameter_map_);
  AudioNode::Trace(visitor);
}

}  // namespace blink
