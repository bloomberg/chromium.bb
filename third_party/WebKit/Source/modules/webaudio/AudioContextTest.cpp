// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioContext.h"

#include "core/dom/Document.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebAudioDevice.h"
#include "public/platform/WebAudioLatencyHint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockWebAudioDevice : public WebAudioDevice {
 public:
  explicit MockWebAudioDevice(double sample_rate, int frames_per_buffer)
      : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
  ~MockWebAudioDevice() override = default;

  void Start() override {}
  void Stop() override {}
  double SampleRate() override { return sample_rate_; }
  int FramesPerBuffer() override { return frames_per_buffer_; }

 private:
  double sample_rate_;
  int frames_per_buffer_;
};

class AudioContextTestPlatform : public TestingPlatformSupport {
 public:
  WebAudioDevice* CreateAudioDevice(unsigned number_of_input_channels,
                                    unsigned number_of_channels,
                                    const WebAudioLatencyHint& latency_hint,
                                    WebAudioDevice::RenderCallback*,
                                    const WebString& device_id,
                                    const WebSecurityOrigin&) override {
    double buffer_size = 0;
    switch (latency_hint.Category()) {
      case WebAudioLatencyHint::kCategoryInteractive:
        buffer_size = AudioHardwareBufferSize();
        break;
      case WebAudioLatencyHint::kCategoryBalanced:
        buffer_size = AudioHardwareBufferSize() * 2;
        break;
      case WebAudioLatencyHint::kCategoryPlayback:
        buffer_size = AudioHardwareBufferSize() * 4;
        break;
      default:
        NOTREACHED();
        break;
    }

    return new MockWebAudioDevice(AudioHardwareSampleRate(), buffer_size);
  }

  double AudioHardwareSampleRate() override { return 44100; }
  size_t AudioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

class AudioContextTest : public ::testing::Test {
 protected:
  void SetUp() override { dummy_page_holder_ = DummyPageHolder::Create(); }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(AudioContextTest, AudioContextOptions_WebAudioLatencyHint) {
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform;

  AudioContextOptions interactive_options;
  interactive_options.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "interactive"));
  AudioContext* interactive_context = AudioContext::Create(
      GetDocument(), interactive_options, ASSERT_NO_EXCEPTION);

  AudioContextOptions balanced_options;
  balanced_options.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "balanced"));
  AudioContext* balanced_context = AudioContext::Create(
      GetDocument(), balanced_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(balanced_context->baseLatency(),
            interactive_context->baseLatency());

  AudioContextOptions playback_options;
  playback_options.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "playback"));
  AudioContext* playback_context = AudioContext::Create(
      GetDocument(), playback_options, ASSERT_NO_EXCEPTION);
  EXPECT_GT(playback_context->baseLatency(), balanced_context->baseLatency());
}

}  // namespace blink
