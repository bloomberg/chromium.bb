// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "core/testing/DummyPageHolder.h"
#include "modules/webaudio/AudioBasicProcessorHandler.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/audio/AudioProcessor.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockAudioProcessor final : public AudioProcessor {
 public:
  MockAudioProcessor() : AudioProcessor(48000, 2) {}
  void Initialize() override { initialized_ = true; }
  void Uninitialize() override { initialized_ = false; }
  void Process(const AudioBus*, AudioBus*, size_t) override {}
  void Reset() override {}
  void SetNumberOfChannels(unsigned) override {}
  unsigned NumberOfChannels() const override { return number_of_channels_; }
  bool RequiresTailProcessing() const override { return true; }
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }
};

class MockProcessorNode final : public AudioNode {
 public:
  MockProcessorNode(BaseAudioContext& context) : AudioNode(context) {
    SetHandler(AudioBasicProcessorHandler::Create(
        AudioHandler::kNodeTypeWaveShaper, *this, 48000,
        WTF::MakeUnique<MockAudioProcessor>()));
    Handler().Initialize();
  }
};

TEST(AudioBasicProcessorHandlerTest, ProcessorFinalization) {
  std::unique_ptr<DummyPageHolder> page = DummyPageHolder::Create();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      &page->GetDocument(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  MockProcessorNode* node = new MockProcessorNode(*context);
  AudioBasicProcessorHandler& handler =
      static_cast<AudioBasicProcessorHandler&>(node->Handler());
  EXPECT_TRUE(handler.Processor());
  EXPECT_TRUE(handler.Processor()->IsInitialized());
  BaseAudioContext::AutoLocker locker(context);
  handler.Dispose();
  // The AudioProcessor should live after dispose() and should not be
  // finalized because an audio thread is using it.
  EXPECT_TRUE(handler.Processor());
  EXPECT_TRUE(handler.Processor()->IsInitialized());
}

}  // namespace blink
