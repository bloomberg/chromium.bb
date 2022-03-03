// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
#include <string.h>

#include "base/memory/weak_ptr.h"
#include "chrome/updater/update_service.h"  // nogncheck

class BrowserUpdaterClient;
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)

@class KeystoneObserver;

// OS X implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterMac : public VersionUpdater {
 public:
  VersionUpdaterMac(const VersionUpdaterMac&) = delete;
  VersionUpdaterMac& operator=(const VersionUpdaterMac&) = delete;

  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback status_callback,
                      PromoteCallback promote_callback) override;
  void PromoteUpdater() const override;

  // Process status updates received from Keystone. The dictionary will contain
  // an AutoupdateStatus value as an intValue at key kAutoupdateStatusStatus. If
  // a version is available (see AutoupdateStatus), it will be present at key
  // kAutoupdateStatusVersion.
  void UpdateStatus(NSDictionary* status);

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterMac();
  ~VersionUpdaterMac() override;

 private:
  // Update the visibility state of promote button.
  void UpdateShowPromoteButton();

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
  // Updates the status from the Chromium Updater.
  void UpdateStatusFromChromiumUpdater(
      VersionUpdater::StatusCallback status_callback,
      VersionUpdater::PromoteCallback promote_callback,
      updater::UpdateService::UpdateState update_state);

  void UpdatePromotionStatusFromChromiumUpdater(
      VersionUpdater::PromoteCallback promote_callback,
      bool enable_promote_button,
      const std::string& version);
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)

  // Callback used to communicate update status to the client.
  StatusCallback status_callback_;

  // Callback used to show or hide the promote UI elements.
  PromoteCallback promote_callback_;

  // The visible state of the promote button.
  bool show_promote_button_;

  // The observer that will receive keystone status updates.
  base::scoped_nsobject<KeystoneObserver> keystone_observer_;

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
  // Instance of the BrowserUpdaterClient used to update the browser with the
  // new updater.
  scoped_refptr<BrowserUpdaterClient> update_client_;
  base::WeakPtrFactory<VersionUpdaterMac> weak_factory_{this};
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_
