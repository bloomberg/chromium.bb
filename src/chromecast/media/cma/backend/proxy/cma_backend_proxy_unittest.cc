// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/cma_backend_proxy.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::StrictMock;

namespace chromecast {
namespace media {
namespace {

class MockCmaBackend : public CmaBackend {
 public:
  MOCK_METHOD0(CreateAudioDecoder, CmaBackend::AudioDecoder*());
  MOCK_METHOD0(CreateVideoDecoder, CmaBackend::VideoDecoder*());
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD1(Start, bool(int64_t));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, bool());
  MOCK_METHOD0(Resume, bool());
  MOCK_METHOD0(GetCurrentPts, int64_t());
  MOCK_METHOD1(SetPlaybackRate, bool(float rate));
  MOCK_METHOD0(LogicalPause, void());
  MOCK_METHOD0(LogicalResume, void());
};

class MockMultizoneAudioDecoderProxy : public MultizoneAudioDecoderProxy {
 public:
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD1(Start, bool(int64_t));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, bool());
  MOCK_METHOD0(Resume, bool());
  MOCK_CONST_METHOD0(GetCurrentPts, int64_t());
  MOCK_METHOD1(SetPlaybackRate, bool(float rate));
  MOCK_METHOD0(LogicalPause, void());
  MOCK_METHOD0(LogicalResume, void());
  MOCK_METHOD1(SetDelegate, void(Delegate*));
  MOCK_METHOD1(PushBuffer, BufferStatus(scoped_refptr<DecoderBufferBase>));
  MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
  MOCK_METHOD1(SetVolume, bool(float));
  MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
  MOCK_METHOD1(GetStatistics, void(Statistics*));
  MOCK_METHOD0(RequiresDecryption, bool());
  MOCK_METHOD1(SetObserver, void(Observer*));
};

}  // namespace

class CmaBackendProxyTest : public testing::Test {
 public:
  CmaBackendProxyTest() {
    auto delegated_video_backend =
        std::make_unique<StrictMock<MockCmaBackend>>();
    auto audio_decoder =
        std::make_unique<StrictMock<MockMultizoneAudioDecoderProxy>>();

    delegated_video_backend_ = delegated_video_backend.get();
    audio_decoder_ = audio_decoder.get();

    CmaBackendProxy::AudioDecoderFactoryCB factory = base::BindOnce(
        [](std::unique_ptr<MultizoneAudioDecoderProxy> ptr) { return ptr; },
        std::move(audio_decoder));
    CmaBackendProxy* proxy = new CmaBackendProxy(
        std::move(delegated_video_backend), std::move(factory));
    backend_.reset(proxy);
  }

 protected:
  void CreateVideoDecoder() {
    EXPECT_CALL(*delegated_video_backend_, CreateVideoDecoder()).Times(1);
    backend_->CreateVideoDecoder();
    testing::Mock::VerifyAndClearExpectations(delegated_video_backend_);
  }

  std::unique_ptr<CmaBackendProxy> backend_;
  MockCmaBackend* delegated_video_backend_;
  MockMultizoneAudioDecoderProxy* audio_decoder_;
};

TEST_F(CmaBackendProxyTest, Initialize) {
  EXPECT_TRUE(backend_->Initialize());

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, Initialize())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Initialize());
  EXPECT_FALSE(backend_->Initialize());
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, Initialize())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*delegated_video_backend_, Initialize())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Initialize());
  EXPECT_FALSE(backend_->Initialize());
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  EXPECT_FALSE(backend_->Initialize());
  EXPECT_FALSE(backend_->Initialize());
}

TEST_F(CmaBackendProxyTest, Start) {
  constexpr float kStartPts = 42;
  EXPECT_TRUE(backend_->Start(kStartPts));

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, Start(kStartPts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, Start(kStartPts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*delegated_video_backend_, Start(kStartPts))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
  EXPECT_FALSE(backend_->Start(kStartPts));
}

TEST_F(CmaBackendProxyTest, Stop) {
  backend_->Stop();

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, Stop());
  backend_->Stop();
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, Stop());
  EXPECT_CALL(*delegated_video_backend_, Stop());
  backend_->Stop();
}

TEST_F(CmaBackendProxyTest, Pause) {
  EXPECT_TRUE(backend_->Pause());

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, Pause())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, Pause())
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*delegated_video_backend_, Pause())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
  EXPECT_FALSE(backend_->Pause());
}

TEST_F(CmaBackendProxyTest, Resume) {
  EXPECT_TRUE(backend_->Resume());

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, Resume())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, Resume())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*delegated_video_backend_, Resume())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
  EXPECT_FALSE(backend_->Resume());
}

TEST_F(CmaBackendProxyTest, GetCurrentPts) {
  EXPECT_EQ(backend_->GetCurrentPts(), std::numeric_limits<int64_t>::min());

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, GetCurrentPts()).WillOnce(Return(42));
  EXPECT_EQ(backend_->GetCurrentPts(), 42);
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, GetCurrentPts())
      .WillOnce(Return(42))
      .WillOnce(Return(42));
  EXPECT_CALL(*delegated_video_backend_, GetCurrentPts())
      .WillOnce(Return(16))
      .WillOnce(Return(360));

  EXPECT_EQ(backend_->GetCurrentPts(), 16);
  EXPECT_EQ(backend_->GetCurrentPts(), 42);
}

TEST_F(CmaBackendProxyTest, SetPlaybackRate) {
  constexpr float kSetPlaybackRatePts = 0.5;
  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, SetPlaybackRate(kSetPlaybackRatePts))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, SetPlaybackRate(kSetPlaybackRatePts))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*delegated_video_backend_, SetPlaybackRate(kSetPlaybackRatePts))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(false));

  EXPECT_TRUE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
  EXPECT_FALSE(backend_->SetPlaybackRate(kSetPlaybackRatePts));
}

TEST_F(CmaBackendProxyTest, LogicalPause) {
  backend_->LogicalPause();

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, LogicalPause());
  backend_->LogicalPause();
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, LogicalPause());
  EXPECT_CALL(*delegated_video_backend_, LogicalPause());
  backend_->LogicalPause();
}

TEST_F(CmaBackendProxyTest, LogicalResume) {
  backend_->LogicalResume();

  ASSERT_EQ(backend_->CreateAudioDecoder(), audio_decoder_);

  EXPECT_CALL(*audio_decoder_, LogicalResume());
  backend_->LogicalResume();
  testing::Mock::VerifyAndClearExpectations(audio_decoder_);

  CreateVideoDecoder();
  EXPECT_CALL(*audio_decoder_, LogicalResume());
  EXPECT_CALL(*delegated_video_backend_, LogicalResume());
  backend_->LogicalResume();
}

}  // namespace media
}  // namespace chromecast
