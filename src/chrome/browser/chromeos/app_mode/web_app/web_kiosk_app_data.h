// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_base.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

struct WebApplicationInfo;

namespace chromeos {

class KioskAppDataDelegate;

class WebKioskAppData : public KioskAppDataBase {
 public:
  enum Status {
    STATUS_INIT,       // Data initialized with app id.
    STATUS_LOADING,    // Loading data from cache or web store.
    STATUS_LOADED,     // Data load finished.
    STATUS_INSTALLED,  // Icon and launch url are fetched and app can be run
                       // without them.
  };
  WebKioskAppData(KioskAppDataDelegate* delegate,
                  const std::string& app_id,
                  const AccountId& account_id,
                  const GURL url,
                  const std::string& title,
                  const GURL icon_url);

  ~WebKioskAppData() override;

  // Loads the locally cached data. Returns true on success.
  bool LoadFromCache();

  // Updates |icon_| from either |KioskAppDataBase::icon_path_| or |icon_url_|.
  void LoadIcon();

  // KioskAppDataBase overrides:
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override;
  void OnIconLoadFailure() override;

  void SetStatus(Status status);

  void UpdateFromWebAppInfo(std::unique_ptr<WebApplicationInfo> app_info);

  Status status() const { return status_; }
  const GURL& install_url() const { return install_url_; }
  const GURL& launch_url() const { return launch_url_; }

 private:
  class IconFetcher;
  void OnDidDownloadIcon(const SkBitmap& icon);

  bool LoadLaunchUrlFromDictionary(const base::Value& dict);

  // Returns the icon url of the icon that was being provided during previous
  // session.
  GURL GetLastIconUrl(const base::Value& dict) const;

  KioskAppDataDelegate* delegate_;  // not owned.
  Status status_;
  const GURL install_url_;  // installation url.
  GURL launch_url_;         // app launch url.

  GURL icon_url_;  // Url of the icon in case nothing is cached.
  // Used to download icon from |icon_url_|.
  std::unique_ptr<IconFetcher> icon_fetcher_;

  base::WeakPtrFactory<WebKioskAppData> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebKioskAppData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_APP_WEB_KIOSK_APP_DATA_H_
