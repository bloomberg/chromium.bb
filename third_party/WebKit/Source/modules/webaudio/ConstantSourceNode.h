// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConstantSourceNode_h
#define ConstantSourceNode_h

#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Threading.h"

namespace blink {

class BaseAudioContext;
class ConstantSourceOptions;
class ExceptionState;

// ConstantSourceNode is an audio generator for a constant source

class ConstantSourceHandler final : public AudioScheduledSourceHandler {
 public:
  static scoped_refptr<ConstantSourceHandler> Create(AudioNode&,
                                                     float sample_rate,
                                                     AudioParamHandler& offset);
  ~ConstantSourceHandler() override;

  // AudioHandler
  void Process(size_t frames_to_process) override;

 private:
  ConstantSourceHandler(AudioNode&,
                        float sample_rate,
                        AudioParamHandler& offset);

  // If we are no longer playing, propogate silence ahead to downstream nodes.
  bool PropagatesSilence() const override;

  scoped_refptr<AudioParamHandler> offset_;
  AudioFloatArray sample_accurate_values_;
};

class ConstantSourceNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ConstantSourceNode* Create(BaseAudioContext&, ExceptionState&);
  static ConstantSourceNode* Create(BaseAudioContext*,
                                    const ConstantSourceOptions&,
                                    ExceptionState&);
  virtual void Trace(blink::Visitor*);

  AudioParam* offset();

 private:
  ConstantSourceNode(BaseAudioContext&);
  ConstantSourceHandler& GetConstantSourceHandler() const;

  Member<AudioParam> offset_;
};

}  // namespace blink

#endif  // ConstantSourceNode_h
