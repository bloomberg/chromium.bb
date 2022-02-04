// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"
#include "chrome/browser/ash/policy/dlp/mock_dlp_content_manager_ash.h"
#endif

namespace {
constexpr char kOrigin[] = "https://origin/";
constexpr blink::mojom::MediaStreamRequestResult kInvalidResult =
    blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED;
}  // namespace

class TabCaptureAccessHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabCaptureAccessHandlerTest() = default;
  ~TabCaptureAccessHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    access_handler_ = std::make_unique<TabCaptureAccessHandler>();
  }

  void ProcessRequest(
      const content::DesktopMediaID& fake_desktop_media_id_response,
      blink::mojom::MediaStreamRequestResult* request_result,
      blink::MediaStreamDevices* devices_result,
      bool expect_result = true) {
    content::MediaStreamRequest request(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), /*page_request_id=*/0,
        GURL(kOrigin), /*user_gesture=*/false, blink::MEDIA_GENERATE_STREAM,
        /*requested_audio_device_id=*/std::string(),
        /*requested_video_device_id=*/std::string(),
        blink::mojom::MediaStreamType::NO_SERVICE,
        blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
        /*disable_local_echo=*/false,
        /*request_pan_tilt_zoom_permission=*/false,
        /*region_capture_capable=*/false);

    base::RunLoop wait_loop;
    content::MediaResponseCallback callback = base::BindOnce(
        [](base::RunLoop* wait_loop, bool expect_result,
           blink::mojom::MediaStreamRequestResult* request_result,
           blink::MediaStreamDevices* devices_result,
           const blink::MediaStreamDevices& devices,
           blink::mojom::MediaStreamRequestResult result,
           std::unique_ptr<content::MediaStreamUI> ui) {
          *request_result = result;
          *devices_result = devices;
          if (!expect_result) {
            FAIL() << "MediaResponseCallback should not be called.";
          }
          wait_loop->Quit();
        },
        &wait_loop, expect_result, request_result, devices_result);
    access_handler_->HandleRequest(web_contents(), request, std::move(callback),
                                   /*extension=*/nullptr);
    if (expect_result) {
      wait_loop.Run();
    } else {
      wait_loop.RunUntilIdle();
    }

    access_handler_.reset();
  }

 protected:
  std::unique_ptr<TabCaptureAccessHandler> access_handler_;
};

TEST_F(TabCaptureAccessHandlerTest, PermissionGiven) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
            devices[0].type);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabCaptureAccessHandlerTest, DlpRestricted) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManagerAsh mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/false);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED, result);
  EXPECT_EQ(0u, devices.size());
}

TEST_F(TabCaptureAccessHandlerTest, DlpNotRestricted) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManagerAsh mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([](const content::DesktopMediaID& media_id,
                   const std::u16string& application_title,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(/*should_proceed=*/true);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices);

  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, result);
  EXPECT_EQ(1u, devices.size());
}

TEST_F(TabCaptureAccessHandlerTest, DlpWebContentsDestroyed) {
  const content::DesktopMediaID source(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents()->GetMainFrame()->GetProcess()->GetID(),
          web_contents()->GetMainFrame()->GetRoutingID()));

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManagerAsh mock_dlp_content_manager;
  policy::ScopedDlpContentManagerAshForTesting scoped_dlp_content_manager(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, CheckScreenShareRestriction)
      .WillOnce([&](const content::DesktopMediaID& media_id,
                    const std::u16string& application_title,
                    base::OnceCallback<void(bool)> callback) {
        DeleteContents();
        std::move(callback).Run(/*should_proceed=*/true);
      });

  extensions::TabCaptureRegistry::Get(profile())->AddRequest(
      web_contents(), /*extension_id=*/"", /*is_anonymous=*/false,
      GURL(kOrigin), source, /*extension_name=*/"", web_contents());

  blink::mojom::MediaStreamRequestResult result = kInvalidResult;
  blink::MediaStreamDevices devices;
  ProcessRequest(source, &result, &devices, /*expect_result=*/false);

  EXPECT_EQ(kInvalidResult, result);
  EXPECT_EQ(0u, devices.size());
}
#endif
