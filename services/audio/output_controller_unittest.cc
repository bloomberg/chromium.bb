// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/environment.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_source_diverter.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Mock;

using media::AudioBus;
using media::AudioManager;
using media::AudioOutputStream;
using media::AudioParameters;
using media::AudioPushSink;
using media::RunClosure;
using media::RunOnceClosure;

namespace audio {

namespace {

constexpr int kSampleRate = AudioParameters::kAudioCDSampleRate;
constexpr int kBitsPerSample = 16;
constexpr media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
constexpr int kSamplesPerPacket = kSampleRate / 1000;
constexpr double kTestVolume = 0.25;
constexpr float kBufferNonZeroData = 1.0f;

}  // namespace

AudioParameters GetTestParams() {
  return AudioParameters(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
}

class MockOutputControllerEventHandler : public OutputController::EventHandler {
 public:
  MockOutputControllerEventHandler() = default;

  MOCK_METHOD0(OnControllerPlaying, void());
  MOCK_METHOD0(OnControllerPaused, void());
  MOCK_METHOD0(OnControllerError, void());
  void OnLog(base::StringPiece) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOutputControllerEventHandler);
};

class MockOutputControllerSyncReader : public OutputController::SyncReader {
 public:
  MockOutputControllerSyncReader() = default;

  MOCK_METHOD3(RequestMoreData,
               void(base::TimeDelta delay,
                    base::TimeTicks delay_timestamp,
                    int prior_frames_skipped));
  MOCK_METHOD1(Read, void(AudioBus* dest));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOutputControllerSyncReader);
};

class MockAudioOutputStream : public AudioOutputStream,
                              public AudioOutputStream::AudioSourceCallback {
 public:
  explicit MockAudioOutputStream(AudioManager* audio_manager)
      : audio_manager_(audio_manager) {}

  // We forward to a fake stream to get automatic OnMoreData callbacks,
  // required by some tests.
  MOCK_METHOD0(DidOpen, void());
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());
  MOCK_METHOD0(DidClose, void());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD1(GetVolume, void(double* volume));

  bool Open() override {
    EXPECT_EQ(nullptr, impl_);
    impl_ =
        audio_manager_->MakeAudioOutputStreamProxy(GetTestParams(), "default");
    impl_->Open();
    DidOpen();
    return true;
  }

  void Start(AudioOutputStream::AudioSourceCallback* cb) override {
    EXPECT_EQ(nullptr, callback_);
    callback_ = cb;
    impl_->Start(this);
    DidStart();
  }

  void Stop() override {
    impl_->Stop();
    callback_ = nullptr;
    DidStop();
  }

  void Close() override {
    if (impl_) {
      impl_->Close();
      impl_ = nullptr;
    }
    DidClose();
  }

 private:
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 AudioBus* dest) override {
    int res = callback_->OnMoreData(delay, delay_timestamp,
                                    prior_frames_skipped, dest);
    EXPECT_EQ(dest->channel(0)[0], kBufferNonZeroData);
    return res;
  }

  void OnError() override {
    // Fake stream doesn't send errors.
    NOTREACHED();
  }

  AudioManager* audio_manager_;
  AudioOutputStream* impl_ = nullptr;
  AudioOutputStream::AudioSourceCallback* callback_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputStream);
};

class MockAudioPushSink : public AudioPushSink {
 public:
  MockAudioPushSink() = default;

  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(OnDataCheck, void(float));

  void OnData(std::unique_ptr<AudioBus> source,
              base::TimeTicks reference_time) override {
    OnDataCheck(source->channel(0)[0]);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioPushSink);
};

ACTION(PopulateBuffer) {
  arg0->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg0->channel(0)[0] = kBufferNonZeroData;
}

class OutputControllerTest : public ::testing::Test {
 public:
  OutputControllerTest()
      : audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<media::TestAudioThread>(false))),
        group_id_(base::UnguessableToken::Create()),
        mock_stream_(audio_manager_.get()) {
    audio_manager_->SetDiverterCallbacks(
        base::BindRepeating(&OutputControllerTest::AddDiverter,
                            base::Unretained(this)),
        base::BindRepeating(&OutputControllerTest::RemoveDiverter,
                            base::Unretained(this)));
  }

  ~OutputControllerTest() override { audio_manager_->Shutdown(); }

 protected:
  void Create() {
    controller_.emplace(audio_manager_.get(), &mock_event_handler_,
                        GetTestParams(), std::string(), group_id_,
                        &mock_sync_reader_);
    controller_->Create(false);
    ASSERT_TRUE(diverter_);
    controller_->SetVolume(kTestVolume);
  }

  void Play() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(3, loop.QuitClosure());
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_sync_reader_, RequestMoreData(_, _, _))
        .WillOnce(RunClosure(barrier))
        .WillRepeatedly(Return());
    EXPECT_CALL(mock_sync_reader_, Read(_))
        .WillOnce(Invoke([barrier](AudioBus* data) {
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
        }))
        .WillRepeatedly(PopulateBuffer());
    controller_->Play();

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
  }

  void PlayWhileDiverting() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(4, loop.QuitClosure());
    EXPECT_CALL(mock_stream_, DidStart()).WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));
    // The mock stream will start pulling data. We verify that the calls are
    // forwarded to SyncReader, and write some data to the buffer that we can
    // verify later.
    EXPECT_CALL(mock_sync_reader_, RequestMoreData(_, _, _))
        .WillOnce(RunClosure(barrier))
        .WillRepeatedly(Return());
    EXPECT_CALL(mock_sync_reader_, Read(_))
        .WillOnce(Invoke([barrier](AudioBus* data) {
          data->channel(0)[0] = kBufferNonZeroData;
          barrier.Run();
        }))
        .WillRepeatedly(PopulateBuffer());
    controller_->Play();

    // Waits for all gmock expectations to be satisfied.
    loop.Run();
    // At some point in the future, the stream must be stopped.
    EXPECT_CALL(mock_stream_, DidStop());
  }

  void Pause() {
    base::RunLoop loop;
    EXPECT_CALL(mock_event_handler_, OnControllerPaused())
        .WillOnce(RunOnceClosure(loop.QuitClosure()));
    controller_->Pause();
    loop.Run();
    Mock::VerifyAndClearExpectations(&mock_event_handler_);
  }

  void ChangeDevice() {
    // Expect the event handler to receive one OnControllerPaying() call and no
    // OnControllerPaused() call.
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);

    // Simulate a device change event to OutputController from the AudioManager.
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&OutputController::OnDeviceChange,
                                  base::Unretained(&(*controller_))));

    // Wait for device change to take effect.
    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void DivertBeforePlaying() {
    EXPECT_CALL(mock_stream_, DidOpen());
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume));

    ASSERT_TRUE(diverter_);
    diverter_->StartDiverting(&mock_stream_);

    base::RunLoop loop;
    // Wait for controller to start diverting.
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void DivertAfterClose() {
    EXPECT_CALL(mock_stream_, DidClose());

    ASSERT_TRUE(diverter_);
    diverter_->StartDiverting(&mock_stream_);

    // Note: Not running the TaskRunner yet. Close() will do that later.
  }

  void DivertWhilePlaying() {
    base::RunLoop loop;
    // The barrier is used to wait for all of the expectations to be fulfilled.
    base::RepeatingClosure barrier =
        base::BarrierClosure(4, loop.QuitClosure());
    // Expect mock streams to be initialized and started.
    EXPECT_CALL(mock_stream_, DidOpen()).WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume))
        .WillOnce(RunClosure(barrier));
    EXPECT_CALL(mock_stream_, DidStart()).WillOnce(RunClosure(barrier));
    // Expect event handler to be informed.
    EXPECT_CALL(mock_event_handler_, OnControllerPlaying())
        .WillOnce(RunClosure(barrier));

    ASSERT_TRUE(diverter_);
    diverter_->StartDiverting(&mock_stream_);
    // Wait until callbacks has started.
    loop.Run();

    // At some point in the future, the stream must be stopped.
    EXPECT_CALL(mock_stream_, DidStop());
  }

  void StartDuplicating(MockAudioPushSink* sink) {
    base::RunLoop loop;
    EXPECT_CALL(*sink, OnDataCheck(kBufferNonZeroData))
        .WillOnce(RunOnceClosure(loop.QuitClosure()))
        .WillRepeatedly(Return());
    ASSERT_TRUE(diverter_);
    diverter_->StartDuplicating(sink);
    loop.Run();
  }

  void StartDuplicatingAfterClose(MockAudioPushSink* sink) {
    EXPECT_CALL(*sink, Close());

    ASSERT_TRUE(diverter_);
    diverter_->StartDuplicating(sink);

    // Note: Not running the TaskRunner yet. Close() will do that later.
  }

  void Revert(bool was_playing) {
    if (was_playing) {
      // Expect the handler to receive one OnControllerPlaying() call as a
      // result of the stream switching back.
      EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    }

    EXPECT_CALL(mock_stream_, DidClose());

    ASSERT_TRUE(diverter_);
    diverter_->StopDiverting();
    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void RevertAfterClose(bool already_expecting_close_call) {
    if (!already_expecting_close_call)
      EXPECT_CALL(mock_stream_, DidClose());

    ASSERT_TRUE(diverter_);
    diverter_->StopDiverting();

    // Note: Not running the TaskRunner yet. Close() will do that later.
  }

  void StopDuplicating(MockAudioPushSink* sink) {
    {
      // First, verify we're still getting callbacks. Must be done on the AM
      // task runner, since it may be a separate thread, and EXPECTing from the
      // main thread would be racy.
      base::RunLoop loop;
      audio_manager_->GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](MockAudioPushSink* sink, base::RepeatingClosure done_closure) {
                Mock::VerifyAndClear(sink);
                EXPECT_CALL(*sink, OnDataCheck(kBufferNonZeroData))
                    .WillOnce(RunClosure(done_closure))
                    .WillRepeatedly(Return());
              },
              sink, loop.QuitClosure()));
      loop.Run();
    }

    {
      EXPECT_CALL(*sink, Close());
      ASSERT_TRUE(diverter_);
      diverter_->StopDuplicating(sink);
      base::RunLoop loop;
      audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
  }

  void StopDuplicatingAfterClose(MockAudioPushSink* sink,
                                 bool already_expecting_close_call) {
    if (!already_expecting_close_call)
      EXPECT_CALL(*sink, Close());

    ASSERT_TRUE(diverter_);
    diverter_->StopDuplicating(sink);

    // Note: Not running the TaskRunner yet. Close() will do that later.
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());
    controller_->Close();

    // Flush any pending tasks (that should have been canceled!).
    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void CloseWithAutoRevert() {
    EXPECT_CALL(mock_stream_, DidClose());
    Close();
  }

  void SimulateErrorThenDeviceChange() {
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&OutputControllerTest::TriggerErrorThenDeviceChange,
                       base::Unretained(this)));

    base::RunLoop loop;
    audio_manager_->GetTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  // These help make test sequences more readable.
  void RevertWasNotPlaying() { Revert(false); }
  void RevertWhilePlaying() { Revert(true); }

  void TriggerErrorThenDeviceChange() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());

    // Errors should be deferred; the device change should ensure it's dropped.
    EXPECT_CALL(mock_event_handler_, OnControllerError()).Times(0);
    controller_->OnError();

    EXPECT_CALL(mock_event_handler_, OnControllerPlaying());
    EXPECT_CALL(mock_event_handler_, OnControllerPaused()).Times(0);
    controller_->OnDeviceChange();
  }

 private:
  void AddDiverter(const base::UnguessableToken& group_id,
                   media::AudioSourceDiverter* diverter) {
    EXPECT_EQ(group_id_, group_id);
    ASSERT_FALSE(diverter_);
    diverter_ = diverter;
    ASSERT_TRUE(diverter_);
  }

  void RemoveDiverter(media::AudioSourceDiverter* diverter) {
    ASSERT_EQ(diverter_, diverter);
    diverter_ = nullptr;
  }

  base::TestMessageLoop message_loop_;
  std::unique_ptr<AudioManager> audio_manager_;
  base::UnguessableToken group_id_;
  StrictMock<MockOutputControllerEventHandler> mock_event_handler_;
  StrictMock<MockOutputControllerSyncReader> mock_sync_reader_;
  StrictMock<MockAudioOutputStream> mock_stream_;
  base::Optional<OutputController> controller_;

  media::AudioSourceDiverter* diverter_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OutputControllerTest);
};

TEST_F(OutputControllerTest, CreateAndClose) {
  Create();
  Close();
}

TEST_F(OutputControllerTest, PlayAndClose) {
  Create();
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlayPauseClose) {
  Create();
  Play();
  Pause();
  Close();
}

TEST_F(OutputControllerTest, PlayPausePlayClose) {
  Create();
  Play();
  Pause();
  Play();
  Close();
}

TEST_F(OutputControllerTest, PlayDeviceChangeClose) {
  Create();
  Play();
  ChangeDevice();
  Close();
}

TEST_F(OutputControllerTest, PlayDeviceChangeError) {
  Create();
  Play();
  SimulateErrorThenDeviceChange();
  Close();
}

TEST_F(OutputControllerTest, PlayDivertRevertClose) {
  Create();
  Play();
  DivertWhilePlaying();
  RevertWhilePlaying();
  Close();
}

TEST_F(OutputControllerTest, PlayDivertRevertDivertRevertClose) {
  Create();
  Play();
  DivertWhilePlaying();
  RevertWhilePlaying();
  DivertWhilePlaying();
  RevertWhilePlaying();
  Close();
}

TEST_F(OutputControllerTest, DivertPlayPausePlayRevertClose) {
  Create();
  DivertBeforePlaying();
  PlayWhileDiverting();
  Pause();
  PlayWhileDiverting();
  RevertWhilePlaying();
  Close();
}

TEST_F(OutputControllerTest, DivertRevertClose) {
  Create();
  DivertBeforePlaying();
  RevertWasNotPlaying();
  Close();
}

TEST_F(OutputControllerTest, PlayDivertCloseRevert) {
  Create();
  Play();
  DivertWhilePlaying();
  RevertAfterClose(false);
  Close();
}

TEST_F(OutputControllerTest, PlayDivertClose) {
  Create();
  Play();
  DivertWhilePlaying();
  CloseWithAutoRevert();
}

TEST_F(OutputControllerTest, PlayCloseDivertRevert) {
  Create();
  Play();
  DivertAfterClose();
  RevertAfterClose(true);
  Close();
}

TEST_F(OutputControllerTest, PlayDuplicateStopClose) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicating(&mock_sink);
  StopDuplicating(&mock_sink);
  Close();
}

TEST_F(OutputControllerTest, PlayDuplicateCloseStop) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicating(&mock_sink);
  StopDuplicatingAfterClose(&mock_sink, false);
  Close();
}

TEST_F(OutputControllerTest, PlayCloseDuplicateStop) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicatingAfterClose(&mock_sink);
  StopDuplicatingAfterClose(&mock_sink, true);
  Close();
}

TEST_F(OutputControllerTest, TwoDuplicates) {
  Create();
  MockAudioPushSink mock_sink_1;
  MockAudioPushSink mock_sink_2;
  Play();
  StartDuplicating(&mock_sink_1);
  StartDuplicating(&mock_sink_2);
  StopDuplicating(&mock_sink_1);
  StopDuplicating(&mock_sink_2);
  Close();
}

TEST_F(OutputControllerTest, DuplicateDivertInteract) {
  Create();
  MockAudioPushSink mock_sink;
  Play();
  StartDuplicating(&mock_sink);
  DivertWhilePlaying();
  StopDuplicating(&mock_sink);
  RevertWhilePlaying();
  Close();
}

}  // namespace audio
