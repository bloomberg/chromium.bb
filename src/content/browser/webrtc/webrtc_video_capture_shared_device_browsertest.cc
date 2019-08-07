// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace content {

namespace {

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kStartVideoCaptureAndVerify[] =
    "startVideoCaptureAndVerifySize(%d, %d)";
static const gfx::Size kVideoSize(320, 200);

}  // namespace

// Integration test sets up a single fake device and obtains a connection to the
// video capture service via the Browser process' service manager. It then
// opens the device from clients. One client is the test calling into the
// video capture service directly. The second client is the Browser, which the
// test exercises through JavaScript.
class WebRtcVideoCaptureSharedDeviceBrowserTest : public ContentBrowserTest {
 public:
  WebRtcVideoCaptureSharedDeviceBrowserTest() : weak_factory_(this) {
    scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
  }

  ~WebRtcVideoCaptureSharedDeviceBrowserTest() override {}

  void OpenDeviceViaService(
      media::VideoCaptureBufferType buffer_type_to_request,
      base::OnceClosure done_cb) {
    connector_->BindInterface(video_capture::mojom::kServiceName,
                              &device_factory_provider_);
    device_factory_provider_->ConnectToVideoSourceProvider(
        mojo::MakeRequest(&video_source_provider_));

    video_source_provider_->GetSourceInfos(base::BindOnce(
        &WebRtcVideoCaptureSharedDeviceBrowserTest::OnSourceInfosReceived,
        weak_factory_.GetWeakPtr(), buffer_type_to_request,
        std::move(done_cb)));
  }

  void OpenDeviceInRendererAndWaitForPlaying() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    embedded_test_server()->StartAcceptingConnections();
    GURL url(embedded_test_server()->GetURL(kVideoCaptureHtmlFile));
    NavigateToURL(shell(), url);

    const std::string javascript_to_execute = base::StringPrintf(
        kStartVideoCaptureAndVerify, kVideoSize.width(), kVideoSize.height());
    std::string result;
    // Start video capture and wait until it started rendering
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(shell(), javascript_to_execute, &result));
    ASSERT_EQ("OK", result);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();

    auto* connection = content::ServiceManagerConnection::GetForProcess();
    ASSERT_TRUE(connection);
    auto* connector = connection->GetConnector();
    ASSERT_TRUE(connector);
    // We need to clone it so that we can use the clone on a different thread.
    connector_ = connector->Clone();

    mock_receiver_ = std::make_unique<video_capture::MockReceiver>(
        mojo::MakeRequest(&receiver_proxy_));
  }

  scoped_refptr<base::TaskRunner> main_task_runner_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<video_capture::MockReceiver> mock_receiver_;

 private:
  void OnSourceInfosReceived(
      media::VideoCaptureBufferType buffer_type_to_request,
      base::OnceClosure done_cb,
      const std::vector<media::VideoCaptureDeviceInfo>& infos) {
    ASSERT_FALSE(infos.empty());
    video_source_provider_->GetVideoSource(infos[0].descriptor.device_id,
                                           mojo::MakeRequest(&video_source_));

    media::VideoCaptureParams requestable_settings;
    ASSERT_FALSE(infos[0].supported_formats.empty());
    requestable_settings.requested_format = infos[0].supported_formats[0];
    requestable_settings.requested_format.frame_size = kVideoSize;
    requestable_settings.buffer_type = buffer_type_to_request;

    video_capture::mojom::PushVideoStreamSubscriptionPtr subscription;
    video_source_->CreatePushSubscription(
        std::move(receiver_proxy_), requestable_settings,
        false /*force_reopen_with_new_settings*/,
        mojo::MakeRequest(&subscription_),
        base::BindOnce(&WebRtcVideoCaptureSharedDeviceBrowserTest::
                           OnCreatePushSubscriptionCallback,
                       weak_factory_.GetWeakPtr(), std::move(done_cb)));
  }

  void OnCreatePushSubscriptionCallback(
      base::OnceClosure done_cb,
      video_capture::mojom::CreatePushSubscriptionResultCode result_code,
      const media::VideoCaptureParams& params) {
    ASSERT_NE(video_capture::mojom::CreatePushSubscriptionResultCode::kFailed,
              result_code);
    subscription_->Activate();
    std::move(done_cb).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  video_capture::mojom::DeviceFactoryProviderPtr device_factory_provider_;
  video_capture::mojom::VideoSourceProviderPtr video_source_provider_;
  video_capture::mojom::VideoSourcePtr video_source_;
  video_capture::mojom::PushVideoStreamSubscriptionPtr subscription_;
  video_capture::mojom::ReceiverPtr receiver_proxy_;
  base::WeakPtrFactory<WebRtcVideoCaptureSharedDeviceBrowserTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCaptureSharedDeviceBrowserTest);
};

// Tests that a single fake video capture device can be opened via JavaScript
// by the Renderer while it is already in use by a direct client of the
// video capture service.
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureSharedDeviceBrowserTest,
    ReceiveFrameInRendererWhileDeviceAlreadyInUseViaDirectServiceClient) {
  Initialize();

  base::RunLoop receive_frame_from_service_wait_loop;
  ON_CALL(*mock_receiver_, DoOnNewBuffer(_, _))
      .WillByDefault(Invoke(
          [](int32_t, media::mojom::VideoBufferHandlePtr* buffer_handle) {
            ASSERT_EQ(
                media::mojom::VideoBufferHandle::Tag::SHARED_BUFFER_HANDLE,
                (*buffer_handle)->which());
          }));
  EXPECT_CALL(*mock_receiver_, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&receive_frame_from_service_wait_loop]() {
        receive_frame_from_service_wait_loop.Quit();
      }))
      .WillRepeatedly(Return());

  base::RunLoop open_device_via_service_run_loop;
  OpenDeviceViaService(media::VideoCaptureBufferType::kSharedMemory,
                       open_device_via_service_run_loop.QuitClosure());
  open_device_via_service_run_loop.Run();

  OpenDeviceInRendererAndWaitForPlaying();

  receive_frame_from_service_wait_loop.Run();
}

#if defined(OS_LINUX)

// Tests that a single fake video capture device can be opened via JavaScript
// by the Renderer while it is already in use by a direct client of the
// video capture service that requested to get buffers as raw file handles.
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureSharedDeviceBrowserTest,
    ReceiveFrameInRendererWhileDeviceAlreadyInUseUsingRawFileHandleBuffers) {
  Initialize();

  base::RunLoop receive_frame_from_service_wait_loop;
  ON_CALL(*mock_receiver_, DoOnNewBuffer(_, _))
      .WillByDefault(Invoke(
          [](int32_t, media::mojom::VideoBufferHandlePtr* buffer_handle) {
            ASSERT_EQ(media::mojom::VideoBufferHandle::Tag::
                          SHARED_MEMORY_VIA_RAW_FILE_DESCRIPTOR,
                      (*buffer_handle)->which());
          }));
  EXPECT_CALL(*mock_receiver_, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&receive_frame_from_service_wait_loop]() {
        receive_frame_from_service_wait_loop.Quit();
      }))
      .WillRepeatedly(Return());

  base::RunLoop open_device_via_service_run_loop;
  OpenDeviceViaService(
      media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor,
      open_device_via_service_run_loop.QuitClosure());
  open_device_via_service_run_loop.Run();

  OpenDeviceInRendererAndWaitForPlaying();

  receive_frame_from_service_wait_loop.Run();
}

// Tests that a single fake video capture device can be opened by a direct
// client of the video capture service that requests to get buffers as raw
// file handles while it is already in use via JavaScript by the Renderer.
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureSharedDeviceBrowserTest,
    ReceiveFrameFromServiceViaRawFileHandlesWhileDeviceAlreadyInUseByRenderer) {
  Initialize();

  OpenDeviceInRendererAndWaitForPlaying();

  base::RunLoop receive_frame_from_service_wait_loop;
  ON_CALL(*mock_receiver_, DoOnNewBuffer(_, _))
      .WillByDefault(Invoke(
          [](int32_t, media::mojom::VideoBufferHandlePtr* buffer_handle) {
            ASSERT_EQ(media::mojom::VideoBufferHandle::Tag::
                          SHARED_MEMORY_VIA_RAW_FILE_DESCRIPTOR,
                      (*buffer_handle)->which());
          }));
  EXPECT_CALL(*mock_receiver_, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillOnce(InvokeWithoutArgs([&receive_frame_from_service_wait_loop]() {
        receive_frame_from_service_wait_loop.Quit();
      }))
      .WillRepeatedly(Return());

  base::RunLoop open_device_via_service_run_loop;
  OpenDeviceViaService(
      media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor,
      open_device_via_service_run_loop.QuitClosure());
  open_device_via_service_run_loop.Run();

  receive_frame_from_service_wait_loop.Run();
}

#endif  // defined(OS_LINUX)

}  // namespace content
