// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GURL;
class Profile;
class ScopedKeepAlive;

// BrowserSerivce's Lacros implementation.
// This handles the requests from ash-chrome.
class BrowserServiceLacros : public crosapi::mojom::BrowserService,
                             public BrowserListObserver {
 public:
  BrowserServiceLacros();
  BrowserServiceLacros(const BrowserServiceLacros&) = delete;
  BrowserServiceLacros& operator=(const BrowserServiceLacros&) = delete;
  ~BrowserServiceLacros() override;

  // crosapi::mojom::BrowserService:
  void REMOVED_0(REMOVED_0Callback callback) override;
  void REMOVED_2(crosapi::mojom::BrowserInitParamsPtr) override;
  void NewWindow(bool incognito,
                 bool should_trigger_session_restore,
                 NewWindowCallback callback) override;
  void NewFullscreenWindow(const GURL& url,
                           NewFullscreenWindowCallback callback) override;
  void NewWindowForDetachingTab(
      const std::u16string& tab_id,
      const std::u16string& group_id,
      NewWindowForDetachingTabCallback callback) override;
  void NewTab(NewTabCallback callback) override;
  void OpenUrl(const GURL& url, OpenUrlCallback callback) override;
  void RestoreTab(RestoreTabCallback callback) override;
  void HandleTabScrubbing(float x_offset) override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override;
  void UpdateDeviceAccountPolicy(const std::vector<uint8_t>& policy) override;
  void UpdateKeepAlive(bool enabled) override;

 private:
  struct PendingOpenUrl;

  void OnSystemInformationReady(
      GetFeedbackDataCallback callback,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);

  void OnGetCompressedHistograms(GetHistogramsCallback callback,
                                 const std::string& compressed_histogram);

  void OpenUrlImpl(Profile* profile, const GURL& url, OpenUrlCallback callback);

  // These *WithProfile() methods are called asynchronously by the corresponding
  // profile-less function, after loading the profile.
  void NewWindowWithProfile(bool incognito,
                            bool should_trigger_session_restore,
                            NewWindowCallback callback,
                            Profile* profile);
  void NewFullscreenWindowWithProfile(const GURL& url,
                                      NewFullscreenWindowCallback callback,
                                      Profile* profile);
  void NewWindowForDetachingTabWithProfile(
      const std::u16string& tab_id,
      const std::u16string& group_id,
      NewWindowForDetachingTabCallback callback,
      Profile* profile);
  void NewTabWithProfile(NewTabCallback callback, Profile* profile);
  void OpenUrlWithProfile(const GURL& url,
                          OpenUrlCallback callback,
                          Profile* profile);
  void RestoreTabWithProfile(RestoreTabCallback callback, Profile* profile);

  // Called when a session is restored.
  void OnSessionRestored(Profile* profile, int num_tabs_restored);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // Keeps the Lacros browser alive in the background. This is destroyed once
  // any browser window is opened.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::vector<PendingOpenUrl> pending_open_urls_;

  base::CallbackListSubscription session_restored_subscription_;
  mojo::Receiver<crosapi::mojom::BrowserService> receiver_{this};
  base::WeakPtrFactory<BrowserServiceLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_BROWSER_SERVICE_LACROS_H_
