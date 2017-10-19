// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletNode_h
#define AudioWorkletNode_h

#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParamMap.h"
#include "modules/webaudio/AudioWorkletNodeOptions.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Threading.h"

namespace blink {

class AudioWorkletProcessor;
class BaseAudioContext;
class CrossThreadAudioParamInfo;
class ExceptionState;

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
  static RefPtr<AudioWorkletHandler> Create(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, RefPtr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions&);

  ~AudioWorkletHandler() override;

  // Called from render thread.
  void Process(size_t frames_to_process) override;

  double TailTime() const override;
  double LatencyTime() const override { return 0; }

  String Name() const { return name_; }

  // Sets |AudioWorkletProcessor|. MUST be called on render thread.
  void SetProcessorOnRenderThread(AudioWorkletProcessor*);

  // Finish |AudioWorkletProcessor| and set the tail time to zero, when
  // the user-supplied |process()| method returns false.
  void FinishProcessorOnRenderThread();

 private:
  AudioWorkletHandler(
      AudioNode&,
      float sample_rate,
      String name,
      HashMap<String, RefPtr<AudioParamHandler>> param_handler_map,
      const AudioWorkletNodeOptions&);

  String name_;

  double tail_time_ = std::numeric_limits<double>::infinity();

  // MUST be set/used by render thread.
  CrossThreadPersistent<AudioWorkletProcessor> processor_;

  HashMap<String, RefPtr<AudioParamHandler>> param_handler_map_;
  HashMap<String, std::unique_ptr<AudioFloatArray>> param_value_map_;
};

class AudioWorkletNode final : public AudioNode,
                               public ActiveScriptWrappable<AudioWorkletNode> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(AudioWorkletNode);

 public:
  static AudioWorkletNode* Create(BaseAudioContext*,
                                  const String& name,
                                  const AudioWorkletNodeOptions&,
                                  ExceptionState&);

  AudioWorkletHandler& GetWorkletHandler() const;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // IDL
  AudioParamMap* parameters() const;

  virtual void Trace(blink::Visitor*);

 private:
  AudioWorkletNode(BaseAudioContext&,
                   const String& name,
                   const AudioWorkletNodeOptions&,
                   const Vector<CrossThreadAudioParamInfo>);

  Member<AudioParamMap> parameter_map_;
};

}  // namespace blink

#endif  // AudioWorkletNode_h
