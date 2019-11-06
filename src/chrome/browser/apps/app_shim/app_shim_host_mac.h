// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace remote_cocoa {
class ApplicationHost;
}  // namespace remote_cocoa

class AppShimHostBootstrap;

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost owns itself, and is
// destroyed when the app it corresponds to is closed or when the channel
// connected to the app shim is closed.
class AppShimHost : public chrome::mojom::AppShimHost {
 public:
  AppShimHost(const std::string& app_id,
              const base::FilePath& profile_path,
              bool uses_remote_views);

  bool UsesRemoteViews() const { return uses_remote_views_; }

  // Returns true if an AppShimHostBootstrap has already connected to this
  // host.
  bool HasBootstrapConnected() const;

  // Invoked to request that the shim be launched (if it has not been launched
  // already).
  void LaunchShim();

  // Invoked when the app shim has launched and connected to the browser.
  virtual void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap);

  // Invoked when the app is closed in the browser process.
  void OnAppClosed();

  // TODO(ccameron): The following three function should directly call the
  // AppShim mojo interface (they only don't due to tests that could be changed
  // to mock the AppShim mojo interface).
  // Invoked when the app should be hidden.
  void OnAppHide();
  // Invoked when a window becomes visible while the app is hidden. Ensures
  // the shim's "Hide/Show" state is updated correctly and the app can be
  // re-hidden.
  virtual void OnAppUnhideWithoutActivation();
  // Invoked when the app is requesting user attention.
  void OnAppRequestUserAttention(apps::AppShimAttentionType type);

  // Functions to allow the handler to determine which app this host corresponds
  // to.
  base::FilePath GetProfilePath() const;
  std::string GetAppId() const;

  // Return the factory to use to create new widgets in the same process.
  remote_cocoa::ApplicationHost* GetRemoteCocoaApplicationHost() const;

  // Return the app shim interface.
  chrome::mojom::AppShim* GetAppShim() const;

 protected:
  // AppShimHost is owned by itself. It will delete itself in Close (called on
  // channel error and OnAppClosed).
  ~AppShimHost() override;

  // Return the AppShimHandler for this app (virtual for tests).
  virtual apps::AppShimHandler* GetAppShimHandler() const;

 private:
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // Closes the channel and destroys the AppShimHost.
  void Close();

  // Helper function to launch the app shim process.
  void LaunchShimInternal(bool recreate_shims);

  // Called when LaunchShim has launched (or failed to launch) a process.
  void OnShimProcessLaunched(bool recreate_shims_requested,
                             base::Process shim_process);

  // Called when a shim process returned via OnShimLaunchCompleted has
  // terminated.
  void OnShimProcessTerminated(bool recreate_shims_requested);

  // chrome::mojom::AppShimHost.
  void FocusApp(apps::AppShimFocusType focus_type,
                const std::vector<base::FilePath>& files) override;
  void SetAppHidden(bool hidden) override;
  void QuitApp() override;

  mojo::Binding<chrome::mojom::AppShimHost> host_binding_;
  chrome::mojom::AppShimPtr app_shim_;
  chrome::mojom::AppShimRequest app_shim_request_;

  // Only allow LaunchShim to have any effect on the first time it is called. If
  // that launch fails, it will re-launch (requesting that the shim be
  // re-created).
  bool launch_shim_has_been_called_;

  std::unique_ptr<AppShimHostBootstrap> bootstrap_;

  std::unique_ptr<remote_cocoa::ApplicationHost> remote_cocoa_application_host_;

  std::string app_id_;
  base::FilePath profile_path_;
  const bool uses_remote_views_;

  // This class is only ever to be used on the UI thread.
  THREAD_CHECKER(thread_checker_);

  // This weak factory is used for launch callbacks only.
  base::WeakPtrFactory<AppShimHost> launch_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AppShimHost);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
