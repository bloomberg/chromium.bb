// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows implementation of version update functionality, used by the WebUI
// About/Help page.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_WIN_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/google/google_update_win.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

class VersionUpdaterWin : public VersionUpdater,
                          public UpdateCheckDelegate {
 public:
  // |owner_widget| is the parent widget hosting the update check UI. Any UI
  // needed to install an update (e.g., a UAC prompt for a system-level install)
  // will be parented to this widget. |owner_widget| may be given a value of
  // nullptr in which case the UAC prompt will be parented to the desktop.
  explicit VersionUpdaterWin(gfx::AcceleratedWidget owner_widget);
  ~VersionUpdaterWin() override;

  // VersionUpdater:
  void CheckForUpdate(const StatusCallback& callback,
                      const PromoteCallback&) override;

  // UpdateCheckDelegate:
  void OnUpdateCheckComplete(const base::string16& new_version) override;
  void OnUpgradeProgress(int progress,
                         const base::string16& new_version) override;
  void OnUpgradeComplete(const base::string16& new_version) override;
  void OnError(GoogleUpdateErrorCode error_code,
               const base::string16& html_error_message,
               const base::string16& new_version) override;

 private:
  void DoBeginUpdateCheck(bool install_update_if_possible);

  // A task run on the UI thread with the result of checking for a pending
  // restart.
  void OnPendingRestartCheck(bool is_update_pending_restart);

  // The widget owning the UI for the update check.
  gfx::AcceleratedWidget owner_widget_;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Used for callbacks.
  base::WeakPtrFactory<VersionUpdaterWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterWin);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_WIN_H_
