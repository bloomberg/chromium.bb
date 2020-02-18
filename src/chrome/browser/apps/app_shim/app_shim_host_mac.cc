// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/common/chrome_features.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/base/ui_base_features.h"

AppShimHost::AppShimHost(AppShimHost::Client* client,
                         const std::string& app_id,
                         const base::FilePath& profile_path,
                         bool uses_remote_views)
    : client_(client),
      host_binding_(this),
      app_shim_request_(mojo::MakeRequest(&app_shim_)),
      launch_shim_has_been_called_(false),
      app_id_(app_id),
      profile_path_(profile_path),
      uses_remote_views_(uses_remote_views),
      launch_weak_factory_(this) {
  // Create the interfaces used to host windows, so that browser windows may be
  // created before the host process finishes launching.
  if (uses_remote_views_ &&
      base::FeatureList::IsEnabled(features::kAppShimRemoteCocoa)) {
    // Create the interface that will be used by views::NativeWidgetMac to
    // create NSWindows hosted in the app shim process.
    remote_cocoa::mojom::ApplicationAssociatedRequest views_application_request;
    remote_cocoa_application_host_ =
        std::make_unique<remote_cocoa::ApplicationHost>(
            &views_application_request);
    app_shim_->CreateRemoteCocoaApplication(
        std::move(views_application_request));
  }
}

AppShimHost::~AppShimHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void AppShimHost::ChannelError(uint32_t custom_reason,
                               const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;

  // OnShimProcessDisconnected will delete |this|.
  client_->OnShimProcessDisconnected(this);
}

void AppShimHost::LaunchShimInternal(bool recreate_shims) {
  DCHECK(launch_shim_has_been_called_);
  DCHECK(!bootstrap_);
  launch_weak_factory_.InvalidateWeakPtrs();
  client_->OnShimLaunchRequested(
      this, recreate_shims,
      base::BindOnce(&AppShimHost::OnShimProcessLaunched,
                     launch_weak_factory_.GetWeakPtr(), recreate_shims),
      base::BindOnce(&AppShimHost::OnShimProcessTerminated,
                     launch_weak_factory_.GetWeakPtr(), recreate_shims));
}

void AppShimHost::OnShimProcessLaunched(bool recreate_shims_requested,
                                        base::Process shim_process) {
  // If a bootstrap connected, then it should have invalidated all weak
  // pointers, preventing this from being called.
  DCHECK(!bootstrap_);

  // If the shim process was created, then await either an AppShimHostBootstrap
  // connecting or the process exiting.
  if (shim_process.IsValid())
    return;

  // Shim launch failing is treated the same as the shim launching but
  // terminating before connecting.
  OnShimProcessTerminated(recreate_shims_requested);
}

void AppShimHost::OnShimProcessTerminated(bool recreate_shims_requested) {
  DCHECK(!bootstrap_);

  // If this was a launch without recreating shims, then the launch may have
  // failed because the shims were not present, or because they were out of
  // date. Try again, recreating the shims this time.
  if (!recreate_shims_requested) {
    DLOG(ERROR) << "Failed to launch shim, attempting to recreate.";
    LaunchShimInternal(true /* recreate_shims */);
    return;
  }

  // If we attempted to recreate the app shims and still failed to launch, then
  // there is no hope to launch the app. Close its windows (since they will
  // never be seen).
  // TODO(https://crbug.com/913362): Consider adding some UI to tell the
  // user that the process launch failed.
  DLOG(ERROR) << "Failed to launch recreated shim, giving up.";

  // OnShimProcessDisconnected will delete |this|.
  client_->OnShimProcessDisconnected(this);
}

////////////////////////////////////////////////////////////////////////////////
// AppShimHost, chrome::mojom::AppShimHost

bool AppShimHost::HasBootstrapConnected() const {
  return bootstrap_ != nullptr;
}

void AppShimHost::OnBootstrapConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  // Prevent any callbacks from any pending launches (e.g, if an internal and
  // external launch happen to race).
  launch_weak_factory_.InvalidateWeakPtrs();

  DCHECK(!bootstrap_);
  bootstrap_ = std::move(bootstrap);
  bootstrap_->OnConnectedToHost(std::move(app_shim_request_));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  host_binding_.Bind(bootstrap_->GetLaunchAppShimHostRequest());
  host_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimHost::ChannelError, base::Unretained(this)));
}

void AppShimHost::LaunchShim() {
  if (launch_shim_has_been_called_)
    return;
  launch_shim_has_been_called_ = true;

  if (bootstrap_) {
    // If there is a connected app shim process, focus the app windows.
    client_->OnShimFocus(this, apps::APP_SHIM_FOCUS_NORMAL,
                         std::vector<base::FilePath>());
  } else {
    // Otherwise, attempt to launch whatever app shims we find.
    LaunchShimInternal(false /* recreate_shims */);
  }
}

void AppShimHost::FocusApp(apps::AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->OnShimFocus(this, focus_type, files);
}

base::FilePath AppShimHost::GetProfilePath() const {
  return profile_path_;
}

std::string AppShimHost::GetAppId() const {
  return app_id_;
}

remote_cocoa::ApplicationHost* AppShimHost::GetRemoteCocoaApplicationHost()
    const {
  return remote_cocoa_application_host_.get();
}

chrome::mojom::AppShim* AppShimHost::GetAppShim() const {
  return app_shim_.get();
}
