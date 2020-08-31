// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/cdm_config.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/clients/mojo_demuxer_stream_impl.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/services/media_service_factory.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

namespace {

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

MATCHER_P(MatchesResult, success, "") {
  return arg->success == success;
}

#if BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kInvalidKeySystem[] = "invalid.key.system";
#endif

const char kSecurityOrigin[] = "https://foo.com";

class MockRendererClient : public mojom::RendererClient {
 public:
  MockRendererClient() = default;
  ~MockRendererClient() override = default;

  // mojom::RendererClient implementation.
  MOCK_METHOD3(OnTimeUpdate,
               void(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time));
  MOCK_METHOD2(OnBufferingStateChange,
               void(BufferingState state, BufferingStateChangeReason reason));
  MOCK_METHOD0(OnEnded, void());
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig&));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig&));
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnStatisticsUpdate,
               void(const media::PipelineStatistics& stats));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnDurationChange, void(base::TimeDelta duration));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRendererClient);
};

ACTION_P(QuitLoop, run_loop) {
  base::PostTask(FROM_HERE, run_loop->QuitClosure());
}

// Tests MediaService using TestMojoMediaClient, which supports CDM creation
// using DefaultCdmFactory (only supports Clear Key key system), and Renderer
// creation using DefaultRendererFactory that always create media::RendererImpl.
class MediaServiceTest : public testing::Test {
 public:
  MediaServiceTest()
      : renderer_client_receiver_(&renderer_client_),
        video_stream_(DemuxerStream::VIDEO) {}
  ~MediaServiceTest() override = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces;
    ignore_result(frame_interfaces.InitWithNewPipeAndPassReceiver());

    media_service_impl_ = CreateMediaServiceForTesting(
        media_service_.BindNewPipeAndPassReceiver());
    media_service_.set_idle_handler(
        base::TimeDelta(),
        base::BindRepeating(&MediaServiceTest::OnMediaServiceIdle,
                            base::Unretained(this)));
    media_service_->CreateInterfaceFactory(
        interface_factory_.BindNewPipeAndPassReceiver(),
        std::move(frame_interfaces));
  }

  MOCK_METHOD3(OnCdmInitialized,
               void(mojom::CdmPromiseResultPtr result,
                    int cdm_id,
                    mojo::PendingRemote<mojom::Decryptor> decryptor));
  MOCK_METHOD0(OnCdmConnectionError, void());

  // Returns the CDM ID associated with the CDM.
  int InitializeCdm(const std::string& key_system, bool expected_result) {
    base::RunLoop run_loop;
    interface_factory_->CreateCdm(key_system,
                                  cdm_.BindNewPipeAndPassReceiver());
    cdm_.set_disconnect_handler(base::BindOnce(
        &MediaServiceTest::OnCdmConnectionError, base::Unretained(this)));

    int cdm_id = CdmContext::kInvalidCdmId;

    // The last parameter mojo::PendingRemote<mojom::Decryptor> is move-only and
    // not supported by DoAll. Hence use WithArg to only extract the "int
    // cdm_id" out and then call DoAll.
    EXPECT_CALL(*this, OnCdmInitialized(MatchesResult(expected_result), _, _))
        .WillOnce(WithArg<1>(DoAll(SaveArg<0>(&cdm_id), QuitLoop(&run_loop))));
    cdm_->Initialize(key_system, url::Origin::Create(GURL(kSecurityOrigin)),
                     CdmConfig(),
                     base::BindOnce(&MediaServiceTest::OnCdmInitialized,
                                    base::Unretained(this)));
    run_loop.Run();
    return cdm_id;
  }

  MOCK_METHOD1(OnRendererInitialized, void(bool));

  void InitializeRenderer(const VideoDecoderConfig& video_config,
                          bool expected_result) {
    base::RunLoop run_loop;
    interface_factory_->CreateDefaultRenderer(
        std::string(), renderer_.BindNewPipeAndPassReceiver());

    video_stream_.set_video_decoder_config(video_config);

    mojo::PendingRemote<mojom::DemuxerStream> video_stream_proxy;
    mojo_video_stream_.reset(new MojoDemuxerStreamImpl(
        &video_stream_, video_stream_proxy.InitWithNewPipeAndPassReceiver()));

    mojo::PendingAssociatedRemote<mojom::RendererClient> client_remote;
    renderer_client_receiver_.Bind(
        client_remote.InitWithNewEndpointAndPassReceiver());

    std::vector<mojo::PendingRemote<mojom::DemuxerStream>> streams;
    streams.push_back(std::move(video_stream_proxy));

    EXPECT_CALL(*this, OnRendererInitialized(expected_result))
        .WillOnce(QuitLoop(&run_loop));
    renderer_->Initialize(
        std::move(client_remote), std::move(streams), nullptr,
        base::BindOnce(&MediaServiceTest::OnRendererInitialized,
                       base::Unretained(this)));
    run_loop.Run();
  }

  MOCK_METHOD0(OnMediaServiceIdle, void());

 protected:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<mojom::MediaService> media_service_;
  mojo::Remote<mojom::InterfaceFactory> interface_factory_;
  mojo::Remote<mojom::ContentDecryptionModule> cdm_;
  mojo::Remote<mojom::Renderer> renderer_;

  std::unique_ptr<MediaService> media_service_impl_;

  NiceMock<MockRendererClient> renderer_client_;
  mojo::AssociatedReceiver<mojom::RendererClient> renderer_client_receiver_;

  StrictMock<MockDemuxerStream> video_stream_;
  std::unique_ptr<MojoDemuxerStreamImpl> mojo_video_stream_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaServiceTest);
};

}  // namespace

// Note: base::RunLoop::RunUntilIdle() does not work well in these tests because
// even when the loop is idle, we may still have pending events in the pipe.
// - If you have an InterfacePtr hosted by the service in the service process,
//   you can use InterfacePtr::FlushForTesting(). Note that this doesn't drain
//   the task runner in the test process and doesn't cover all negative cases.
// - If you expect a callback on an InterfacePtr call or connection error, use
//   base::RunLoop::Run() and QuitLoop().

// TODO(crbug.com/829233): Enable these tests on Android.
#if BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)
TEST_F(MediaServiceTest, InitializeCdm_Success) {
  InitializeCdm(kClearKeyKeySystem, true);
}

TEST_F(MediaServiceTest, InitializeCdm_InvalidKeySystem) {
  InitializeCdm(kInvalidKeySystem, false);
}
#endif  // BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
TEST_F(MediaServiceTest, InitializeRenderer) {
  InitializeRenderer(TestVideoConfig::Normal(), true);
}
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

TEST_F(MediaServiceTest, InterfaceFactoryPreventsIdling) {
  // The service should not idle during this operation.
  interface_factory_.FlushForTesting();

  // Disconnecting InterfaceFactory will cause the service to idle since there
  // are no media components hosted and so no other interfaces should be
  // connected.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnMediaServiceIdle()).WillOnce(QuitLoop(&run_loop));
  interface_factory_.reset();
  run_loop.Run();
}

#if (BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)) || \
    BUILDFLAG(ENABLE_MOJO_RENDERER)
// MediaService stays alive as long as there are InterfaceFactory impls, which
// are then deferred destroyed until no media components (e.g. CDM or Renderer)
// are hosted.
TEST_F(MediaServiceTest, Idling) {
#if BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)
  InitializeCdm(kClearKeyKeySystem, true);
#endif

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  InitializeRenderer(TestVideoConfig::Normal(), true);
#endif

  // Disconnecting CDM and Renderer services doesn't terminate MediaService
  // since |interface_factory_| is still alive.
  cdm_.reset();
  renderer_.reset();
  interface_factory_.FlushForTesting();

  // Disconnecting InterfaceFactory will cause the service to idle since no
  // other interfaces are connected.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnMediaServiceIdle()).WillOnce(QuitLoop(&run_loop));
  interface_factory_.reset();
  run_loop.Run();
}

TEST_F(MediaServiceTest, MoreIdling) {
#if BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)
  InitializeCdm(kClearKeyKeySystem, true);
#endif

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  InitializeRenderer(TestVideoConfig::Normal(), true);
#endif

  ASSERT_TRUE(cdm_ || renderer_);

  // Disconnecting InterfaceFactory should not terminate the MediaService since
  // there are still media components (CDM or Renderer) hosted.
  interface_factory_.reset();
  if (cdm_)
    cdm_.FlushForTesting();
  else if (renderer_)
    renderer_.FlushForTesting();
  else
    NOTREACHED();

  // Disconnecting CDM and Renderer will cause the service to idle since no
  // other interfaces are connected.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnMediaServiceIdle()).WillOnce(QuitLoop(&run_loop));
  cdm_.reset();
  renderer_.reset();
  run_loop.Run();
}
#endif  // (BUILDFLAG(ENABLE_MOJO_CDM) && !defined(OS_ANDROID)) ||
        //  BUILDFLAG(ENABLE_MOJO_RENDERER)

}  // namespace media
