// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_controller.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/user_input_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using base::WaitableEvent;

namespace audio {

namespace {

const int kSampleRate = media::AudioParameters::kAudioCDSampleRate;
const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
const int kSamplesPerPacket = kSampleRate / 10;

const double kMaxVolume = 1.0;

// InputController will poll once every second, so wait at most a bit
// more than that for the callbacks.
constexpr base::TimeDelta kOnMuteWaitTimeout =
    base::TimeDelta::FromMilliseconds(1500);

// Posts run_loop->QuitClosure() on specified
// message loop after a certain number of calls given by |limit|.
ACTION_P4(CheckCountAndPostQuitTask, count, limit, loop_or_proxy, run_loop) {
  if (++*count >= limit) {
    loop_or_proxy->PostTask(FROM_HERE, run_loop->QuitWhenIdleClosure());
  }
}

void RunLoopWithTimeout(base::RunLoop* run_loop, base::TimeDelta timeout) {
  base::OneShotTimer timeout_timer;
  timeout_timer.Start(FROM_HERE, timeout, run_loop->QuitClosure());
  run_loop->Run();
}

}  // namespace

class MockInputControllerEventHandler : public InputController::EventHandler {
 public:
  MockInputControllerEventHandler() = default;

  void OnLog(base::StringPiece) override {}

  MOCK_METHOD1(OnCreated, void(bool initially_muted));
  MOCK_METHOD1(OnError, void(InputController::ErrorCode error_code));
  MOCK_METHOD1(OnMuted, void(bool is_muted));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputControllerEventHandler);
};

class MockSyncWriter : public InputController::SyncWriter {
 public:
  MockSyncWriter() = default;

  MOCK_METHOD4(Write,
               void(const media::AudioBus* data,
                    double volume,
                    bool key_pressed,
                    base::TimeTicks capture_time));
  MOCK_METHOD0(Close, void());
};

class MockUserInputMonitor : public media::UserInputMonitor {
 public:
  MockUserInputMonitor() = default;

  uint32_t GetKeyPressCount() const override { return 0; }

  MOCK_METHOD0(EnableKeyPressMonitoring, void());
  MOCK_METHOD0(DisableKeyPressMonitoring, void());
};

class MockAudioInputStream : public media::AudioInputStream {
 public:
  MockAudioInputStream() {}
  ~MockAudioInputStream() override {}

  void Start(AudioInputCallback*) override {}
  void Stop() override {}
  void Close() override {}
  double GetMaxVolume() override { return kMaxVolume; }
  double GetVolume() override { return 0; }
  bool SetAutomaticGainControl(bool) override { return false; }
  bool GetAutomaticGainControl() override { return false; }
  bool IsMuted() override { return false; }
  void SetOutputDeviceForAec(const std::string&) override {}

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double));
};

class InputControllerTest : public testing::TestWithParam<bool> {
 public:
  InputControllerTest()
      : run_on_audio_thread_(GetParam()),
        audio_manager_(std::make_unique<media::FakeAudioManager>(
            std::make_unique<media::TestAudioThread>(!run_on_audio_thread_),
            &log_factory_)),
        params_(media::AudioParameters::AUDIO_FAKE,
                kChannelLayout,
                kSampleRate,
                kSamplesPerPacket) {}

  ~InputControllerTest() override {
    audio_manager_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void CreateAudioController() {
    controller_ = InputController::Create(
        audio_manager_.get(), &event_handler_, &sync_writer_,
        &user_input_monitor_, params_,
        media::AudioDeviceDescription::kDefaultDeviceId, false);
  }

  void CloseAudioController() {
    if (run_on_audio_thread_) {
      controller_->Close(base::OnceClosure());
      return;
    }

    base::RunLoop loop;
    controller_->Close(loop.QuitWhenIdleClosure());
    loop.Run();
  }

  base::MessageLoop message_loop_;

  // Parameterize tests to run InputController either on audio thread
  // (synchronously), or on a different thread (non-blocking).
  bool run_on_audio_thread_;

  scoped_refptr<InputController> controller_;
  media::FakeAudioLogFactory log_factory_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  MockInputControllerEventHandler event_handler_;
  MockSyncWriter sync_writer_;
  MockUserInputMonitor user_input_monitor_;
  media::AudioParameters params_;
  MockAudioInputStream stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputControllerTest);
};

TEST_P(InputControllerTest, CreateAndCloseWithoutRecording) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

// Test a normal call sequence of create, record and close.
TEST_P(InputControllerTest, CreateRecordAndClose) {
  int count = 0;

  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  base::RunLoop loop;

  // Write() should be called ten times.
  EXPECT_CALL(sync_writer_, Write(NotNull(), _, _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, message_loop_.task_runner(), &loop));
  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();

  // Record and wait until ten Write() callbacks are received.
  loop.Run();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();
}

// Test that InputController rejects insanely large packet sizes.
TEST_P(InputControllerTest, SamplesPerPacketTooLarge) {
  // Create an audio device with a very large packet size.
  params_.set_frames_per_buffer(kSamplesPerPacket * 1000);

  // OnCreated() shall not be called in this test.
  EXPECT_CALL(event_handler_, OnCreated(_)).Times(Exactly(0));
  CreateAudioController();
  ASSERT_FALSE(controller_.get());
}

TEST_P(InputControllerTest, CloseTwice) {
  EXPECT_CALL(event_handler_, OnCreated(_));
  CreateAudioController();
  ASSERT_TRUE(controller_.get());

  EXPECT_CALL(user_input_monitor_, EnableKeyPressMonitoring());
  controller_->Record();

  EXPECT_CALL(user_input_monitor_, DisableKeyPressMonitoring());
  EXPECT_CALL(sync_writer_, Close());
  CloseAudioController();

  CloseAudioController();
}

// Test that InputController sends OnMute callbacks properly.
TEST_P(InputControllerTest, TestOnmutedCallbackInitiallyUnmuted) {
  const auto timeout = kOnMuteWaitTimeout;

  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop unmute_run_loop;
  base::RunLoop mute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler_, OnCreated(false)).WillOnce(InvokeWithoutArgs([&] {
    setup_run_loop.QuitWhenIdle();
  }));
  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(event_handler_, OnMuted(true)).WillOnce(InvokeWithoutArgs([&] {
    mute_run_loop.Quit();
  }));
  EXPECT_CALL(event_handler_, OnMuted(false)).WillOnce(InvokeWithoutArgs([&] {
    unmute_run_loop.Quit();
  }));

  media::FakeAudioInputStream::SetGlobalMutedState(false);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  RunLoopWithTimeout(&setup_run_loop, timeout);

  media::FakeAudioInputStream::SetGlobalMutedState(true);
  RunLoopWithTimeout(&mute_run_loop, timeout);
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController();
}

TEST_P(InputControllerTest, TestOnmutedCallbackInitiallyMuted) {
  const auto timeout = kOnMuteWaitTimeout;

  WaitableEvent callback_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop unmute_run_loop;
  base::RunLoop setup_run_loop;
  EXPECT_CALL(event_handler_, OnCreated(true)).WillOnce(InvokeWithoutArgs([&] {
    setup_run_loop.QuitWhenIdle();
  }));
  EXPECT_CALL(sync_writer_, Close());
  EXPECT_CALL(event_handler_, OnMuted(false)).WillOnce(InvokeWithoutArgs([&] {
    unmute_run_loop.Quit();
  }));

  media::FakeAudioInputStream::SetGlobalMutedState(true);
  CreateAudioController();
  ASSERT_TRUE(controller_.get());
  RunLoopWithTimeout(&setup_run_loop, timeout);

  media::FakeAudioInputStream::SetGlobalMutedState(false);
  RunLoopWithTimeout(&unmute_run_loop, timeout);

  CloseAudioController();
}

INSTANTIATE_TEST_CASE_P(SyncAsync, InputControllerTest, testing::Bool());

}  // namespace audio
