// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/stable_video_decoder_factory_service.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace media {

namespace {

VideoDecoderConfig CreateValidVideoDecoderConfig() {
  const VideoDecoderConfig config(
      VideoCodec::kH264, VideoCodecProfile::H264PROFILE_BASELINE,
      VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace::REC709(),
      VideoTransformation(VIDEO_ROTATION_90, /*mirrored=*/true),
      /*coded_size=*/gfx::Size(640, 368),
      /*visible_rect=*/gfx::Rect(1, 1, 630, 360),
      /*natural_size=*/gfx::Size(1260, 720),
      /*extra_data=*/std::vector<uint8_t>{1, 2, 3},
      EncryptionScheme::kUnencrypted);
  DCHECK(config.IsValidConfig());
  return config;
}

class MockVideoFrameHandleReleaser : public mojom::VideoFrameHandleReleaser {
 public:
  explicit MockVideoFrameHandleReleaser(
      mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser)
      : video_frame_handle_releaser_receiver_(
            this,
            std::move(video_frame_handle_releaser)) {}
  MockVideoFrameHandleReleaser(const MockVideoFrameHandleReleaser&) = delete;
  MockVideoFrameHandleReleaser& operator=(const MockVideoFrameHandleReleaser&) =
      delete;
  ~MockVideoFrameHandleReleaser() override = default;

  // mojom::VideoFrameHandleReleaser implementation.
  MOCK_METHOD2(ReleaseVideoFrame,
               void(const base::UnguessableToken& release_token,
                    const gpu::SyncToken& release_sync_token));

 private:
  mojo::Receiver<mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_receiver_;
};

class MockVideoDecoder : public mojom::VideoDecoder {
 public:
  MockVideoDecoder() = default;
  MockVideoDecoder(const MockVideoDecoder&) = delete;
  MockVideoDecoder& operator=(const MockVideoDecoder&) = delete;
  ~MockVideoDecoder() override = default;

  mojo::AssociatedRemote<mojom::VideoDecoderClient> TakeClientRemote() {
    return std::move(client_remote_);
  }
  mojo::Remote<mojom::MediaLog> TakeMediaLogRemote() {
    return std::move(media_log_remote_);
  }
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
  TakeVideoFrameHandleReleaser() {
    return std::move(video_frame_handle_releaser_);
  }
  std::unique_ptr<MojoDecoderBufferReader> TakeMojoDecoderBufferReader() {
    return std::move(mojo_decoder_buffer_reader_);
  };

  // mojom::VideoDecoder implementation.
  MOCK_METHOD1(GetSupportedConfigs, void(GetSupportedConfigsCallback callback));
  void Construct(
      mojo::PendingAssociatedRemote<mojom::VideoDecoderClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log,
      mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser,
      mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
      mojom::CommandBufferIdPtr command_buffer_id,
      const gfx::ColorSpace& target_color_space) final {
    client_remote_.Bind(std::move(client));
    media_log_remote_.Bind(std::move(media_log));
    video_frame_handle_releaser_ =
        std::make_unique<StrictMock<MockVideoFrameHandleReleaser>>(
            std::move(video_frame_handle_releaser));
    DoConstruct(std::move(command_buffer_id), target_color_space);
  }
  MOCK_METHOD2(DoConstruct,
               void(mojom::CommandBufferIdPtr command_buffer_id,
                    const gfx::ColorSpace& target_color_space));
  MOCK_METHOD4(Initialize,
               void(const VideoDecoderConfig& config,
                    bool low_delay,
                    const absl::optional<base::UnguessableToken>& cdm_id,
                    InitializeCallback callback));
  MOCK_METHOD2(Decode,
               void(mojom::DecoderBufferPtr buffer, DecodeCallback callback));
  MOCK_METHOD1(Reset, void(ResetCallback callback));
  MOCK_METHOD1(OnOverlayInfoChanged, void(const OverlayInfo& overlay_info));

 private:
  mojo::AssociatedRemote<mojom::VideoDecoderClient> client_remote_;
  mojo::Remote<mojom::MediaLog> media_log_remote_;
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      video_frame_handle_releaser_;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;
};

class MockStableVideoDecoderClient : public stable::mojom::VideoDecoderClient {
 public:
  explicit MockStableVideoDecoderClient(
      mojo::PendingAssociatedReceiver<stable::mojom::VideoDecoderClient>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockStableVideoDecoderClient(const MockStableVideoDecoderClient&) = delete;
  MockStableVideoDecoderClient& operator=(const MockStableVideoDecoderClient&) =
      delete;
  ~MockStableVideoDecoderClient() override = default;

  // stable::mojom::VideoDecoderClient implementation.
  MOCK_METHOD3(OnVideoFrameDecoded,
               void(const scoped_refptr<VideoFrame>& frame,
                    bool can_read_without_stalling,
                    const base::UnguessableToken& release_token));
  MOCK_METHOD1(OnWaiting, void(WaitingReason reason));

 private:
  mojo::AssociatedReceiver<stable::mojom::VideoDecoderClient> receiver_;
};

class MockStableMediaLog : public stable::mojom::MediaLog {
 public:
  explicit MockStableMediaLog(
      mojo::PendingReceiver<stable::mojom::MediaLog> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockStableMediaLog(const MockStableMediaLog&) = delete;
  MockStableMediaLog& operator=(const MockStableMediaLog&) = delete;
  ~MockStableMediaLog() override = default;

  // stable::mojom::MediaLog implementation.
  MOCK_METHOD1(AddLogRecord, void(const MediaLogRecord& event));

 private:
  mojo::Receiver<stable::mojom::MediaLog> receiver_;
};

// AuxiliaryEndpoints groups the endpoints that support the operation of a
// StableVideoDecoderService and that come from the Construct() call. That way,
// tests can easily poke at one endpoint and set expectations on the other. For
// example, a test might want to simulate the scenario in which a frame has been
// decoded by the underlying mojom::VideoDecoder. In this case, the test can
// call |video_decoder_client_remote|->OnVideoFrameDecoded() and then set an
// expectation on |mock_stable_video_decoder_client|->OnVideoFrameDecoded().
struct AuxiliaryEndpoints {
  // |video_decoder_client_remote| is the client that the underlying
  // mojom::VideoDecoder receives through the Construct() call. Tests can make
  // calls on it and those calls should ultimately be received by the
  // |mock_stable_video_decoder_client|.
  mojo::AssociatedRemote<mojom::VideoDecoderClient> video_decoder_client_remote;
  std::unique_ptr<StrictMock<MockStableVideoDecoderClient>>
      mock_stable_video_decoder_client;

  // |media_log_remote| is the MediaLog that the underlying mojom::VideoDecoder
  // receives through the Construct() call. Tests can make calls on it and those
  // calls should ultimately be received by the |mock_stable_media_log|.
  mojo::Remote<mojom::MediaLog> media_log_remote;
  std::unique_ptr<StrictMock<MockStableMediaLog>> mock_stable_media_log;

  // Tests can use |stable_video_frame_handle_releaser_remote| to simulate
  // releasing a VideoFrame.
  // |mock_video_frame_handle_releaser| is the VideoFrameHandleReleaser that's
  // setup when the underlying mojom::VideoDecoder receives a Construct() call.
  // Tests can make calls on |stable_video_frame_handle_releaser_remote| and
  // they should be ultimately received by the
  // |mock_video_frame_handle_releaser|.
  mojo::Remote<stable::mojom::VideoFrameHandleReleaser>
      stable_video_frame_handle_releaser_remote;
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      mock_video_frame_handle_releaser;

  // |mojo_decoder_buffer_reader| wraps the reading end of the data pipe that
  // the underlying mojom::VideoDecoder receives through the Construct() call.
  // Tests can write data using the |mojo_decoder_buffer_writer| and that data
  // should be ultimately received by the |mojo_decoder_buffer_reader|.
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader;
};

// Calls Construct() on |stable_video_decoder_remote| and, if
// |expect_construct_call| is true, expects a corresponding Construct() call on
// |mock_video_decoder| which is assumed to be the backing decoder of
// |stable_video_decoder_remote|. Returns nullptr if the expectations on
// |mock_video_decoder| are violated. Otherwise, returns an AuxiliaryEndpoints
// instance that contains the supporting endpoints that tests can use to
// interact with the auxiliary interfaces used by the
// |stable_video_decoder_remote|.
std::unique_ptr<AuxiliaryEndpoints> ConstructStableVideoDecoder(
    mojo::Remote<stable::mojom::StableVideoDecoder>&
        stable_video_decoder_remote,
    StrictMock<MockVideoDecoder>& mock_video_decoder,
    bool expect_construct_call) {
  constexpr gfx::ColorSpace kTargetColorSpace = gfx::ColorSpace::CreateSRGB();
  if (expect_construct_call) {
    EXPECT_CALL(mock_video_decoder,
                DoConstruct(/*command_buffer_id=*/_,
                            /*target_color_space=*/kTargetColorSpace));
  }
  mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
      stable_video_decoder_client_remote;
  auto mock_stable_video_decoder_client =
      std::make_unique<StrictMock<MockStableVideoDecoderClient>>(
          stable_video_decoder_client_remote
              .InitWithNewEndpointAndPassReceiver());

  mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote;
  auto mock_stable_media_log = std::make_unique<StrictMock<MockStableMediaLog>>(
      stable_media_log_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<stable::mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_remote;

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer =
      MojoDecoderBufferWriter::Create(
          GetDefaultDecoderBufferConverterCapacity(DemuxerStream::VIDEO),
          &remote_consumer_handle);

  stable_video_decoder_remote->Construct(
      std::move(stable_video_decoder_client_remote),
      std::move(stable_media_log_remote),
      video_frame_handle_releaser_remote.BindNewPipeAndPassReceiver(),
      std::move(remote_consumer_handle), kTargetColorSpace);
  stable_video_decoder_remote.FlushForTesting();

  if (!Mock::VerifyAndClearExpectations(&mock_video_decoder))
    return nullptr;

  auto auxiliary_endpoints = std::make_unique<AuxiliaryEndpoints>();

  auxiliary_endpoints->video_decoder_client_remote =
      mock_video_decoder.TakeClientRemote();
  auxiliary_endpoints->mock_stable_video_decoder_client =
      std::move(mock_stable_video_decoder_client);

  auxiliary_endpoints->media_log_remote =
      mock_video_decoder.TakeMediaLogRemote();
  auxiliary_endpoints->mock_stable_media_log = std::move(mock_stable_media_log);

  auxiliary_endpoints->stable_video_frame_handle_releaser_remote =
      std::move(video_frame_handle_releaser_remote);
  auxiliary_endpoints->mock_video_frame_handle_releaser =
      mock_video_decoder.TakeVideoFrameHandleReleaser();

  auxiliary_endpoints->mojo_decoder_buffer_writer =
      std::move(mojo_decoder_buffer_writer);
  auxiliary_endpoints->mojo_decoder_buffer_reader =
      mock_video_decoder.TakeMojoDecoderBufferReader();

  return auxiliary_endpoints;
}

class StableVideoDecoderServiceTest : public testing::Test {
 public:
  StableVideoDecoderServiceTest() {
    stable_video_decoder_factory_service_
        .SetVideoDecoderCreationCallbackForTesting(
            video_decoder_creation_cb_.Get());
  }

  StableVideoDecoderServiceTest(const StableVideoDecoderServiceTest&) = delete;
  StableVideoDecoderServiceTest& operator=(
      const StableVideoDecoderServiceTest&) = delete;
  ~StableVideoDecoderServiceTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory>
        stable_video_decoder_factory_receiver;
    stable_video_decoder_factory_remote_ =
        mojo::Remote<stable::mojom::StableVideoDecoderFactory>(
            stable_video_decoder_factory_receiver
                .InitWithNewPipeAndPassRemote());
    stable_video_decoder_factory_service_.BindReceiver(
        std::move(stable_video_decoder_factory_receiver));
    ASSERT_TRUE(stable_video_decoder_factory_remote_.is_connected());
  }

 protected:
  mojo::Remote<stable::mojom::StableVideoDecoder> CreateStableVideoDecoder(
      std::unique_ptr<StrictMock<MockVideoDecoder>> dst_video_decoder) {
    // Each CreateStableVideoDecoder() should result in exactly one call to the
    // video decoder creation callback, i.e., the
    // StableVideoDecoderFactoryService should not re-use mojom::VideoDecoder
    // implementation instances.
    EXPECT_CALL(video_decoder_creation_cb_, Run(_, _))
        .WillOnce(Return(ByMove(std::move(dst_video_decoder))));
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder>
        stable_video_decoder_receiver;
    mojo::Remote<stable::mojom::StableVideoDecoder> video_decoder_remote(
        stable_video_decoder_receiver.InitWithNewPipeAndPassRemote());
    stable_video_decoder_factory_remote_->CreateStableVideoDecoder(
        std::move(stable_video_decoder_receiver));
    stable_video_decoder_factory_remote_.FlushForTesting();
    if (!Mock::VerifyAndClearExpectations(&video_decoder_creation_cb_))
      return {};
    return video_decoder_remote;
  }

  base::test::TaskEnvironment task_environment_;
  StrictMock<base::MockRepeatingCallback<std::unique_ptr<
      mojom::VideoDecoder>(MojoMediaClient*, MojoCdmServiceContext*)>>
      video_decoder_creation_cb_;
  StableVideoDecoderFactoryService stable_video_decoder_factory_service_;
  mojo::Remote<stable::mojom::StableVideoDecoderFactory>
      stable_video_decoder_factory_remote_;
  mojo::Remote<stable::mojom::StableVideoDecoder> stable_video_decoder_remote_;
};

// Tests that we can create multiple StableVideoDecoder implementation instances
// through the StableVideoDecoderFactory and that they can exist concurrently.
TEST_F(StableVideoDecoderServiceTest, FactoryCanCreateStableVideoDecoders) {
  std::vector<mojo::Remote<stable::mojom::StableVideoDecoder>>
      stable_video_decoder_remotes;
  constexpr size_t kNumConcurrentDecoders = 5u;
  for (size_t i = 0u; i < kNumConcurrentDecoders; i++) {
    auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
    auto stable_video_decoder_remote =
        CreateStableVideoDecoder(std::move(mock_video_decoder));
    stable_video_decoder_remotes.push_back(
        std::move(stable_video_decoder_remote));
  }
  for (const auto& remote : stable_video_decoder_remotes) {
    ASSERT_TRUE(remote.is_bound());
    ASSERT_TRUE(remote.is_connected());
  }
}

// Tests that a call to stable::mojom::VideoDecoder::Construct() gets routed
// correctly to the underlying mojom::VideoDecoder.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanBeConstructed) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  ASSERT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/true));
}

// Tests that if two calls to stable::mojom::VideoDecoder::Construct() are made,
// only one is routed to the underlying mojom::VideoDecoder.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotBeConstructedTwice) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  EXPECT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/true));
  EXPECT_TRUE(ConstructStableVideoDecoder(stable_video_decoder_remote,
                                          *mock_video_decoder_raw,
                                          /*expect_construct_call=*/false));
}

// Tests that a call to stable::mojom::VideoDecoder::Initialize() gets routed
// correctly to the underlying mojom::VideoDecoder. Also tests that when the
// underlying mojom::VideoDecoder calls the initialization callback, the call
// gets routed to the client.
TEST_F(StableVideoDecoderServiceTest, StableVideoDecoderCanBeInitialized) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  VideoDecoderConfig received_config;
  constexpr bool kLowDelay = true;
  constexpr absl::optional<base::UnguessableToken> kCdmId = absl::nullopt;
  StrictMock<base::MockOnceCallback<void(
      const media::DecoderStatus& status, bool needs_bitstream_conversion,
      int32_t max_decode_requests, VideoDecoderType decoder_type)>>
      initialize_cb_to_send;
  mojom::VideoDecoder::InitializeCallback received_initialize_cb;
  const DecoderStatus kDecoderStatus = DecoderStatus::Codes::kAborted;
  constexpr bool kNeedsBitstreamConversion = true;
  constexpr int32_t kMaxDecodeRequests = 123;
  constexpr VideoDecoderType kDecoderType = VideoDecoderType::kVda;

  EXPECT_CALL(*mock_video_decoder_raw,
              Initialize(/*config=*/_, kLowDelay, kCdmId,
                         /*callback=*/_))
      .WillOnce([&](const VideoDecoderConfig& config, bool low_delay,
                    const absl::optional<base::UnguessableToken>& cdm_id,
                    mojom::VideoDecoder::InitializeCallback callback) {
        received_config = config;
        received_initialize_cb = std::move(callback);
      });
  EXPECT_CALL(initialize_cb_to_send,
              Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
                  kDecoderType));
  stable_video_decoder_remote->Initialize(
      config_to_send, kLowDelay,
      mojo::PendingRemote<stable::mojom::StableCdmContext>(),
      initialize_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(mock_video_decoder_raw));

  std::move(received_initialize_cb)
      .Run(kDecoderStatus, kNeedsBitstreamConversion, kMaxDecodeRequests,
           kDecoderType);
  task_environment_.RunUntilIdle();
}

// Tests that the StableVideoDecoderService rejects a call to
// stable::mojom::VideoDecoder::Initialize() before
// stable::mojom::VideoDecoder::Construct() gets called.
TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderCannotBeInitializedBeforeConstruction) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());

  const VideoDecoderConfig config_to_send = CreateValidVideoDecoderConfig();
  constexpr bool kLowDelay = true;
  StrictMock<base::MockOnceCallback<void(
      const media::DecoderStatus& status, bool needs_bitstream_conversion,
      int32_t max_decode_requests, VideoDecoderType decoder_type)>>
      initialize_cb_to_send;

  EXPECT_CALL(initialize_cb_to_send,
              Run(DecoderStatus(DecoderStatus::Codes::kFailed),
                  /*needs_bitstream_conversion=*/false,
                  /*max_decode_requests=*/1, VideoDecoderType::kUnknown));
  stable_video_decoder_remote->Initialize(
      config_to_send, kLowDelay,
      mojo::PendingRemote<stable::mojom::StableCdmContext>(),
      initialize_cb_to_send.Get());
  stable_video_decoder_remote.FlushForTesting();
}

TEST_F(StableVideoDecoderServiceTest,
       StableVideoDecoderClientReceivesOnVideoFrameDecodedEvent) {
  auto mock_video_decoder = std::make_unique<StrictMock<MockVideoDecoder>>();
  auto* mock_video_decoder_raw = mock_video_decoder.get();
  auto stable_video_decoder_remote =
      CreateStableVideoDecoder(std::move(mock_video_decoder));
  ASSERT_TRUE(stable_video_decoder_remote.is_bound());
  ASSERT_TRUE(stable_video_decoder_remote.is_connected());
  auto auxiliary_endpoints = ConstructStableVideoDecoder(
      stable_video_decoder_remote, *mock_video_decoder_raw,
      /*expect_construct_call=*/true);
  ASSERT_TRUE(auxiliary_endpoints);
  ASSERT_TRUE(auxiliary_endpoints->video_decoder_client_remote);
  ASSERT_TRUE(auxiliary_endpoints->mock_stable_video_decoder_client);

  const auto token_for_release = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> video_frame_to_send = VideoFrame::CreateEOSFrame();
  scoped_refptr<VideoFrame> video_frame_received;
  constexpr bool kCanReadWithoutStalling = true;
  EXPECT_CALL(
      *auxiliary_endpoints->mock_stable_video_decoder_client,
      OnVideoFrameDecoded(_, kCanReadWithoutStalling, token_for_release))
      .WillOnce(SaveArg<0>(&video_frame_received));
  auxiliary_endpoints->video_decoder_client_remote->OnVideoFrameDecoded(
      video_frame_to_send, kCanReadWithoutStalling, token_for_release);
  auxiliary_endpoints->video_decoder_client_remote.FlushForTesting();
  ASSERT_TRUE(video_frame_received);
  EXPECT_TRUE(video_frame_received->metadata().end_of_stream);
}

}  // namespace

}  // namespace media
