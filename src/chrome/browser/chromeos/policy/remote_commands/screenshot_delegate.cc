// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/screenshot_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/upload_job_impl.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

ScreenshotDelegate::ScreenshotDelegate() {}

ScreenshotDelegate::~ScreenshotDelegate() {}

bool ScreenshotDelegate::IsScreenshotAllowed() {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();
  // DeviceCloudPolicyManagerChromeOS and StatusUploader can be null during
  // shutdown (and unit tests) - don't allow screenshots unless we have a
  // StatusUploader that can confirm that screenshots are allowed.
  return manager && manager->GetStatusUploader() &&
         manager->GetStatusUploader()->IsSessionDataUploadAllowed();
}

void ScreenshotDelegate::TakeSnapshot(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    ui::GrabWindowSnapshotAsyncPNGCallback callback) {
  ui::GrabWindowSnapshotAsyncPNG(
      window, source_rect,
      base::BindOnce(&ScreenshotDelegate::StoreScreenshot,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<UploadJob> ScreenshotDelegate::CreateUploadJob(
    const GURL& upload_url,
    UploadJob::Delegate* delegate) {
  DeviceOAuth2TokenService* device_oauth2_token_service =
      DeviceOAuth2TokenServiceFactory::Get();

  CoreAccountId robot_account_id =
      device_oauth2_token_service->GetRobotAccountId();

  SYSLOG(INFO) << "Creating upload job for screenshot";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("remote_command_screenshot", R"(
        semantics {
          sender: "Chrome OS Remote Commands"
          description: "Admins of kiosks are able to request screenshots "
              "of the current screen shown on the kiosk, which is uploaded to "
              "the device management server."
          trigger: "Admin requests remote screenshot on the Admin Console."
          data: "Screenshot of the current screen shown on the kiosk."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings, however this "
              "request will only happen on explicit admin request and when no "
              "user interaction or media capture happened since last reboot."
          policy_exception_justification: "Requires explicit admin action."
        }
      )");
  return std::unique_ptr<UploadJob>(new UploadJobImpl(
      upload_url, robot_account_id,
      device_oauth2_token_service->GetAccessTokenManager(),
      g_browser_process->shared_url_loader_factory(), delegate,
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator),
      traffic_annotation, base::ThreadTaskRunnerHandle::Get()));
}

void ScreenshotDelegate::StoreScreenshot(
    ui::GrabWindowSnapshotAsyncPNGCallback callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  std::move(callback).Run(png_data);
}

}  // namespace policy
