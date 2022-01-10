// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace apps {

struct AppLaunchParams;

// AppPublisher parent class (in the App Service sense) for all app publishers.
// See components/services/app_service/README.md.
//
// TODO(crbug.com/1253250): Add other mojom publisher functions.
class AppPublisher {
 public:
  explicit AppPublisher(AppServiceProxy* proxy);
  AppPublisher(const AppPublisher&) = delete;
  AppPublisher& operator=(const AppPublisher&) = delete;
  ~AppPublisher();

  // Returns an app object from the provided parameters
  static std::unique_ptr<App> MakeApp(AppType app_type,
                                      const std::string& app_id,
                                      Readiness readiness,
                                      const std::string& name);

  // Registers this AppPublisher to AppServiceProxy, allowing it to receive App
  // Service API calls. This function must be called after the object's
  // creation, and can't be called in the constructor function to avoid
  // receiving API calls before being fully constructed and ready. This should
  // be called immediately before the first call to AppPublisher::Publish that
  // sends the initial list of apps to the App Service.
  void RegisterPublisher(AppType app_type);

  // Requests an icon for an app identified by |app_id|. The icon is identified
  // by |icon_key| and parameterised by |icon_type| and |size_hint_in_dp|. If
  // |allow_placeholder_icon| is true, a default placeholder icon can be
  // returned if no other icon is available. Calls |callback| with the result.
  //
  // Publishers implementing this method should:
  //  - provide an icon as closely sized to |size_hint_in_dp| as possible
  //  - load from the specific resource ID if |icon_key.resource_id| is set
  //  - may optionally use |icon_key|'s timeline property as a "version number"
  //    for an icon. Alternatively, this may be ignored if there will only ever
  //    be one version of an icon at any time.
  //  - optionally return a placeholder default icon if |allow_placeholder_icon|
  //    is true and when no icon is available for the app (or an icon for the
  //    app cannot be efficiently provided). Otherwise, a null icon should be
  //    returned.
  virtual void LoadIcon(const std::string& app_id,
                        const IconKey& icon_key,
                        apps::IconType icon_type,
                        int32_t size_hint_in_dip,
                        bool allow_placeholder_icon,
                        LoadIconCallback callback) = 0;

  // Launches an app with |params|.
  //
  // Publishers implementing this method should:
  // - Launch the app represent by the |params.app_id|.
  // - Launch the app with all the params that is applicable to the platform.
  // - Return launch_result if applicable.
  virtual void LaunchAppWithParams(AppLaunchParams&& params,
                                   LaunchCallback callback) = 0;

 protected:
  // Publish one `app` to AppServiceProxy. Should be called whenever the app
  // represented by `app` undergoes some state change to inform AppServiceProxy
  // of the change. Ensure that RegisterPublisher() has been called before the
  // first call to this method.
  void Publish(std::unique_ptr<App> app);

  // Publish multiple `apps` to AppServiceProxy. Should be called whenever the
  // app represented by `app` undergoes some state change to inform
  // AppServiceProxy of the change. Ensure that RegisterPublisher() has been
  // called before the first call to this method.
  void Publish(std::vector<std::unique_ptr<App>> apps);

  AppServiceProxy* proxy() { return proxy_; }

 private:
  raw_ptr<AppServiceProxy> proxy_ = nullptr;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_APP_PUBLISHER_H_
