// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APPS_APK_WEB_APP_INSTALLER_H_
#define CHROME_BROWSER_ASH_APPS_APK_WEB_APP_INSTALLER_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class Profile;

namespace web_app {
enum class InstallResultCode;
}

namespace ash {

// Class which takes an arc::mojom::WebAppInfo struct and decodes the data
// within it to install as a local web app.
class ApkWebAppInstaller {
 public:
  using InstallFinishCallback = base::OnceCallback<void(
      const web_app::AppId&,
      const bool is_web_only_twa,
      const absl::optional<std::string> sha256_fingerprint,
      web_app::InstallResultCode)>;

  // Do nothing class purely for the purpose of allowing us to specify
  // a WeakPtr<Owner> member as a proxy for a profile lifetime observer.
  class Owner {};

  ApkWebAppInstaller(const ApkWebAppInstaller&) = delete;
  ApkWebAppInstaller& operator=(const ApkWebAppInstaller&) = delete;

  // Installs a web app represented by |web_app_info| with icon bytes
  // |icon_png_data| into |profile|. |owner| must stay alive for this class to
  // finish its tasks, otherwise installation will be aborted. Runs |callback|
  // with either the id of the installed web app if installation was successful,
  // or an empty id if not.
  static void Install(Profile* profile,
                      arc::mojom::WebAppInfoPtr web_app_info,
                      arc::mojom::RawIconPngDataPtr icon,
                      InstallFinishCallback callback,
                      base::WeakPtr<Owner> weak_owner);

 protected:
  // This class owns itself and deletes itself when it is finished.
  ApkWebAppInstaller(Profile* profile,
                     InstallFinishCallback callback,
                     base::WeakPtr<Owner> weak_owner);
  virtual ~ApkWebAppInstaller();

  // Starts the installation flow by decoding icon data.
  void Start(arc::mojom::WebAppInfoPtr web_app_info,
             arc::mojom::RawIconPngDataPtr icon);

  // Calls |callback_| with |id|, and deletes this object. Virtual for testing.
  virtual void CompleteInstallation(const web_app::AppId& id,
                                    web_app::InstallResultCode code);

  // Callback method for installation completed response.
  void OnWebAppCreated(const GURL& start_url,
                       const web_app::AppId& app_id,
                       web_app::InstallResultCode code);

  // Callback method for data_decoder::DecodeImage.
  void OnImageDecoded(const SkBitmap& decoded_image);

  // Run the installation. Virtual for testing.
  virtual void DoInstall();

  bool has_web_app_info() const { return web_app_info_ != nullptr; }
  const WebApplicationInfo& web_app_info() const { return *web_app_info_; }

 private:
  // If |weak_owner_| is ever invalidated while this class is working,
  // installation will be aborted. |weak_owner_|'s lifetime must be equal to or
  // shorter than that of |profile_|.
  Profile* profile_;
  bool is_web_only_twa_;
  absl::optional<std::string> sha256_fingerprint_;
  InstallFinishCallback callback_;
  base::WeakPtr<Owner> weak_owner_;

  std::unique_ptr<WebApplicationInfo> web_app_info_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APPS_APK_WEB_APP_INSTALLER_H_
