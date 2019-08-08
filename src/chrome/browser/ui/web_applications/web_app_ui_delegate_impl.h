// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/web_applications/components/web_app_ui_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class Browser;

namespace web_app {

class WebAppUiDelegateImpl : public KeyedService,
                             public BrowserListObserver,
                             public WebAppUiDelegate {
 public:
  static WebAppUiDelegateImpl* Get(Profile* profile);

  explicit WebAppUiDelegateImpl(Profile* profile);
  ~WebAppUiDelegateImpl() override;

  // KeyedService
  void Shutdown() override;

  // WebAppUiDelegate
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::Optional<AppId> GetAppIdForBrowser(Browser* browser);

  Profile* profile_;

  std::map<AppId, std::vector<base::OnceClosure>> windows_closed_requests_map_;
  std::map<AppId, size_t> num_windows_for_apps_map_;

  DISALLOW_COPY_AND_ASSIGN(WebAppUiDelegateImpl);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_
