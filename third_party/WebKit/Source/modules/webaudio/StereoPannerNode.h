// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StereoPannerNode_h
#define StereoPannerNode_h

#include "base/gtest_prod_util.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioParam.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/StereoPanner.h"
#include <memory>

namespace blink {

class BaseAudioContext;
class StereoPannerOptions;

// StereoPannerNode is an AudioNode with one input and one output. It is
// specifically designed for equal-power stereo panning.
class StereoPannerHandler final : public AudioHandler {
 public:
  static PassRefPtr<StereoPannerHandler> Create(AudioNode&,
                                                float sample_rate,
                                                AudioParamHandler& pan);
  ~StereoPannerHandler() override;

  void Process(size_t frames_to_process) override;
  void ProcessOnlyAudioParams(size_t frames_to_process) override;
  void Initialize() override;

  void SetChannelCount(unsigned long, ExceptionState&) final;
  void SetChannelCountMode(const String&, ExceptionState&) final;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

 private:
  StereoPannerHandler(AudioNode&, float sample_rate, AudioParamHandler& pan);

  std::unique_ptr<StereoPanner> stereo_panner_;
  RefPtr<AudioParamHandler> pan_;

  AudioFloatArray sample_accurate_pan_values_;

  FRIEND_TEST_ALL_PREFIXES(StereoPannerNodeTest, StereoPannerLifetime);
};

class StereoPannerNode final : public AudioNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StereoPannerNode* Create(BaseAudioContext&, ExceptionState&);
  static StereoPannerNode* Create(BaseAudioContext*,
                                  const StereoPannerOptions&,
                                  ExceptionState&);
  DECLARE_VIRTUAL_TRACE();

  AudioParam* pan() const;

 private:
  StereoPannerNode(BaseAudioContext&);

  Member<AudioParam> pan_;
};

}  // namespace blink

#endif  // StereoPannerNode_h
