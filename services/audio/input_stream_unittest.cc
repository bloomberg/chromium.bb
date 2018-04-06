// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_stream.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "media/audio/audio_io.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/user_input_monitor.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/audio/test/mock_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace audio {

namespace {

const uint32_t kDefaultSharedMemoryCount = 10;
const bool kDoEnableAGC = true;
const bool kDoNotEnableAGC = false;
const bool kValidStream = true;
const bool kInvalidStream = false;
const char* kDefaultDeviceId = "default";

class MockStreamClient : public media::mojom::AudioInputStreamClient {
 public:
  MockStreamClient() : binding_(this) {}

  media::mojom::AudioInputStreamClientPtr MakePtr() {
    media::mojom::AudioInputStreamClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    binding_.set_connection_error_handler(base::BindOnce(
        &MockStreamClient::BindingConnectionError, base::Unretained(this)));
    return ptr;
  }

  void CloseBinding() { binding_.Close(); }

  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnMutedStateChanged, void(bool));
  MOCK_METHOD0(BindingConnectionError, void());

 private:
  mojo::Binding<media::mojom::AudioInputStreamClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockStreamClient);
};

class MockUserInputMonitor : public media::UserInputMonitor {
 public:
  MockUserInputMonitor() {}

  size_t GetKeyPressCount() const override { return 0; }

  MOCK_METHOD0(StartKeyboardMonitoring, void());
  MOCK_METHOD0(StopKeyboardMonitoring, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUserInputMonitor);
};

class MockStream : public media::AudioInputStream {
 public:
  MockStream() {}

  double GetMaxVolume() override { return 1; }

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioInputCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD0(IsMuted, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStream);
};

}  // namespace

class AudioServiceInputStreamTest : public testing::Test {
 public:
  AudioServiceInputStreamTest()
      : audio_manager_(std::make_unique<media::TestAudioThread>(false)) {}

  ~AudioServiceInputStreamTest() { audio_manager_.Shutdown(); }

  void SetUp() override {
    mojo::edk::SetDefaultProcessErrorCallback(
        base::BindRepeating(&AudioServiceInputStreamTest::BadMessageCallback,
                            base::Unretained(this)));
  }

  void TearDown() override {
    mojo::edk::SetDefaultProcessErrorCallback(
        mojo::edk::ProcessErrorCallback());
  }

  std::unique_ptr<InputStream> CreateStream(
      media::mojom::AudioInputStreamRequest request,
      bool enable_agc) {
    return std::make_unique<InputStream>(
        base::BindOnce(&AudioServiceInputStreamTest::OnCreated,
                       base::Unretained(this)),
        base::BindOnce(&AudioServiceInputStreamTest::DeleteCallback,
                       base::Unretained(this)),
        std::move(request), client_.MakePtr(), log_.MakePtr(), &audio_manager_,
        &user_input_monitor_, media::AudioParameters::UnavailableDeviceParams(),
        kDefaultDeviceId, kDefaultSharedMemoryCount, enable_agc);
  }

  media::MockAudioManager& audio_manager() { return audio_manager_; }

  MockStreamClient& client() { return client_; }

  MockLog& log() { return log_; }

  void OnCreated(media::mojom::AudioDataPipePtr ptr, bool initially_muted) {
    CreatedCallback(!!ptr);
  }

  MOCK_METHOD1(CreatedCallback, void(bool /*valid*/));
  MOCK_METHOD1(DeleteCallback, void(InputStream*));
  MOCK_METHOD1(BadMessageCallback, void(const std::string&));

 private:
  base::test::ScopedTaskEnvironment scoped_task_env_;
  media::MockAudioManager audio_manager_;
  StrictMock<MockStreamClient> client_;
  NiceMock<MockLog> log_;
  NiceMock<MockUserInputMonitor> user_input_monitor_;

  DISALLOW_COPY_AND_ASSIGN(AudioServiceInputStreamTest);
};

TEST_F(AudioServiceInputStreamTest, ConstructDestruct) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructStreamAndCloseClientBinding_DestructsStream) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(*this, DeleteCallback(stream.get()));
  client().CloseBinding();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructStreamAndResetStreamPtr_DestructsStream) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));

  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(*this, DeleteCallback(stream.get()));
  stream_ptr.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, Record) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Start(NotNull()));
  EXPECT_CALL(log(), OnStarted());
  stream_ptr->Record();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Stop());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetVolume) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  const double new_volume = 0.618;
  EXPECT_CALL(mock_stream, SetVolume(new_volume));
  EXPECT_CALL(log(), OnSetVolume(new_volume));
  stream_ptr->SetVolume(new_volume);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetNegativeVolume_BadMessage) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, BadMessageCallback(_));
  EXPECT_CALL(*this, DeleteCallback(stream.get()));
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  stream_ptr->SetVolume(-0.618);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, SetVolumeGreaterThanOne_BadMessage) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, BadMessageCallback(_));
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(*this, DeleteCallback(stream.get()));
  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(client(), BindingConnectionError());
  stream_ptr->SetVolume(1.618);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest, CreateStreamWithAGCEnable_PropagateAGC) {
  NiceMock<MockStream> mock_stream;
  audio_manager().SetMakeInputStreamCB(base::BindRepeating(
      [](media::AudioInputStream* stream, const media::AudioParameters& params,
         const std::string& device_id) { return stream; },
      &mock_stream));

  EXPECT_CALL(mock_stream, Open()).WillOnce(Return(true));
  EXPECT_CALL(mock_stream, SetAutomaticGainControl(kDoEnableAGC));
  EXPECT_CALL(log(), OnCreated(_, _));
  EXPECT_CALL(*this, CreatedCallback(kValidStream));
  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoEnableAGC);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(log(), OnClosed());
  EXPECT_CALL(mock_stream, Close());
  EXPECT_CALL(client(), BindingConnectionError());
  stream.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioServiceInputStreamTest,
       ConstructWithStreamCreationFailure_SignalsError) {
  // By default, MockAudioManager fails to create a stream.

  media::mojom::AudioInputStreamPtr stream_ptr;
  std::unique_ptr<InputStream> stream =
      CreateStream(mojo::MakeRequest(&stream_ptr), kDoNotEnableAGC);

  EXPECT_CALL(*this, CreatedCallback(kInvalidStream));
  EXPECT_CALL(log(), OnError());
  EXPECT_CALL(client(), OnError());
  EXPECT_CALL(client(), BindingConnectionError());
  EXPECT_CALL(*this, DeleteCallback(stream.get()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace audio
