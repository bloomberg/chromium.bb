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
  explicit MockWebAudioDevice(double sampleRate, int framesPerBuffer)
      : m_sampleRate(sampleRate), m_framesPerBuffer(framesPerBuffer) {}
  ~MockWebAudioDevice() override = default;

  void start() override {}
  void stop() override {}
  double sampleRate() override { return m_sampleRate; }
  int framesPerBuffer() override { return m_framesPerBuffer; }

 private:
  double m_sampleRate;
  int m_framesPerBuffer;
};

class AudioContextTestPlatform : public TestingPlatformSupport {
 public:
  WebAudioDevice* createAudioDevice(unsigned numberOfInputChannels,
                                    unsigned numberOfChannels,
                                    const WebAudioLatencyHint& latencyHint,
                                    WebAudioDevice::RenderCallback*,
                                    const WebString& deviceId,
                                    const WebSecurityOrigin&) override {
    double bufferSize = 0;
    switch (latencyHint.category()) {
      case WebAudioLatencyHint::kCategoryInteractive:
        bufferSize = audioHardwareBufferSize();
        break;
      case WebAudioLatencyHint::kCategoryBalanced:
        bufferSize = audioHardwareBufferSize() * 2;
        break;
      case WebAudioLatencyHint::kCategoryPlayback:
        bufferSize = audioHardwareBufferSize() * 4;
        break;
      default:
        NOTREACHED();
        break;
    }

    return new MockWebAudioDevice(audioHardwareSampleRate(), bufferSize);
  }

  double audioHardwareSampleRate() override { return 44100; }
  size_t audioHardwareBufferSize() override { return 128; }
};

}  // anonymous namespace

class AudioContextTest : public ::testing::Test {
 protected:
  void SetUp() override { m_dummyPageHolder = DummyPageHolder::create(); }

  Document& document() { return m_dummyPageHolder->document(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(AudioContextTest, AudioContextOptions_WebAudioLatencyHint) {
  ScopedTestingPlatformSupport<AudioContextTestPlatform> platform;

  AudioContextOptions interactiveOptions;
  interactiveOptions.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "interactive"));
  AudioContext* interactiveContext =
      AudioContext::create(document(), interactiveOptions, ASSERT_NO_EXCEPTION);

  AudioContextOptions balancedOptions;
  balancedOptions.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "balanced"));
  AudioContext* balancedContext =
      AudioContext::create(document(), balancedOptions, ASSERT_NO_EXCEPTION);
  EXPECT_GT(balancedContext->baseLatency(), interactiveContext->baseLatency());

  AudioContextOptions playbackOptions;
  playbackOptions.setLatencyHint(
      AudioContextLatencyCategoryOrDouble::fromAudioContextLatencyCategory(
          "playback"));
  AudioContext* playbackContext =
      AudioContext::create(document(), playbackOptions, ASSERT_NO_EXCEPTION);
  EXPECT_GT(playbackContext->baseLatency(), balancedContext->baseLatency());
}

}  // namespace blink
