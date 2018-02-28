// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletNode_h
#define AudioWorkletNode_h

#include "base/memory/scoped_refptr.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParamMap.h"
#include "modules/webaudio/AudioWorkletNodeOptions.h"
#include "modules/webaudio/AudioWorkletProcessorErrorState.h"
#include "platform/wtf/Threading.h"

namespace blink {

class AudioNodeInput;
class AudioWorkletProcessor;
class BaseAudioContext;
class CrossThreadAudioParamInfo;
class ExceptionState;
class MessagePort;
class ScriptState;

// AudioWorkletNode is a user-facing interface of custom audio processor in
// Web Audio API. The integration of WebAudio renderer is done via
// AudioWorkletHandler and the actual audio processing runs on
// AudioWorkletProcess.
//
//               [Main Scope]                   |    [AudioWorkletGlobalScope]
//  AudioWorkletNode <-> AudioWorkletHandler <==|==>   AudioWorkletProcessor
//   (JS interface)       (Renderer access)     |      (V8 audio processing)

class AudioWorkletHandler final : public AudioHandler {
 public:
  static scoped_refptr<AudioWorkletHandler> Create(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions&);

  ~AudioWorkletHandler() override;

  // Called from render thread.
  void Process(size_t frames_to_process) override;

  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  double TailTime() const override;
  double LatencyTime() const override { return 0; }

  String Name() const { return name_; }

  // Sets |AudioWorkletProcessor| and changes the state of the processor.
  // MUST be called from the render thread.
  void SetProcessorOnRenderThread(AudioWorkletProcessor*);

  // Finish |AudioWorkletProcessor| and set the tail time to zero, when
  // the user-supplied |process()| method returns false.
  void FinishProcessorOnRenderThread();

  void NotifyProcessorError(AudioWorkletProcessorErrorState);

 private:
  AudioWorkletHandler(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions&);

  String name_;

  double tail_time_ = std::numeric_limits<double>::infinity();

  // MUST be set/used by render thread.
  CrossThreadPersistent<AudioWorkletProcessor> processor_;

  HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map_;
  HashMap<String, std::unique_ptr<AudioFloatArray>> param_value_map_;

  // A reference to the main thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class AudioWorkletNode final : public AudioNode,
                               public ActiveScriptWrappable<AudioWorkletNode> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(AudioWorkletNode);

 public:
  static AudioWorkletNode* Create(ScriptState*,
                                  BaseAudioContext*,
                                  const String& name,
                                  const AudioWorkletNodeOptions&,
                                  ExceptionState&);

  AudioWorkletHandler& GetWorkletHandler() const;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // IDL
  AudioParamMap* parameters() const;
  MessagePort* port() const;
  DEFINE_ATTRIBUTE_EVENT_LISTENER(processorerror);

  void FireProcessorError();

  virtual void Trace(blink::Visitor*);

 private:
  AudioWorkletNode(BaseAudioContext&,
                   const String& name,
                   const AudioWorkletNodeOptions&,
                   const Vector<CrossThreadAudioParamInfo>,
                   MessagePort* node_port);

  Member<AudioParamMap> parameter_map_;
  Member<MessagePort> node_port_;
};

}  // namespace blink

#endif  // AudioWorkletNode_h
