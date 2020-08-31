// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_SETUP_CONTROLLER_H_

#include <list>
#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

// Test AndroidSmsAppSetupController implementation.
class FakeAndroidSmsAppSetupController : public AndroidSmsAppSetupController {
 public:
  FakeAndroidSmsAppSetupController();
  ~FakeAndroidSmsAppSetupController() override;

  struct AppMetadata {
    AppMetadata();
    AppMetadata(const AppMetadata& other);
    ~AppMetadata();

    web_app::AppId pwa;
    bool is_cookie_present = true;
  };

  // Returns null if no app has been installed at |install_url|.
  const AppMetadata* GetAppMetadataAtUrl(const GURL& install_url) const;

  // If |id_for_app| is provided, this function installs an app with the given
  // ID at |install_url|. Otherwise, this function removes any existing app at
  // that URL.
  void SetAppAtUrl(const GURL& install_url,
                   const base::Optional<web_app::AppId>& id_for_app);

  // Completes a pending setup request (i.e., a previous call to SetUpApp()).
  // If |id_for_app| is set, the request is successful and the installed app
  // will have the provided ID; if |id_for_app| is nullopt, the request fails.
  void CompletePendingSetUpAppRequest(
      const GURL& expected_app_url,
      const GURL& expected_install_url,
      const base::Optional<web_app::AppId>& id_for_app);

  // Completes a pending cookie deletion request (i.e., a previous call to
  // DeleteRememberDeviceByDefaultCookie()).
  void CompletePendingDeleteCookieRequest(const GURL& expected_app_url,
                                          const GURL& expected_install_url);

  // Completes a pending app removal request (i.e., a previous call to
  // RemoveApp()). If |success| is true, the app will be removed; otherwise, the
  // app will remain in place.
  void CompleteRemoveAppRequest(const GURL& expected_app_url,
                                const GURL& expected_install_url,
                                const GURL& expected_migrated_to_app_url,
                                bool success);

 private:
  // AndroidSmsAppSetupController:
  void SetUpApp(const GURL& app_url,
                const GURL& install_url,
                SuccessCallback callback) override;
  base::Optional<web_app::AppId> GetPwa(const GURL& install_url) override;
  void DeleteRememberDeviceByDefaultCookie(const GURL& app_url,
                                           SuccessCallback callback) override;
  void RemoveApp(const GURL& app_url,
                 const GURL& install_url,
                 const GURL& migrated_to_app_url,
                 SuccessCallback callback) override;

  using SetUpAppData = std::tuple<GURL, GURL, SuccessCallback>;
  std::list<std::unique_ptr<SetUpAppData>> pending_set_up_app_requests_;

  using RemoveAppData = std::tuple<GURL, GURL, GURL, SuccessCallback>;
  std::list<std::unique_ptr<RemoveAppData>> pending_remove_app_requests_;

  using DeleteCookieData = std::pair<GURL, SuccessCallback>;
  std::list<std::unique_ptr<DeleteCookieData>> pending_delete_cookie_requests_;

  base::flat_map<GURL, AppMetadata> install_url_to_metadata_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppSetupController);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
