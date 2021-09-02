// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_INSTALL_OPTIONS_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

using WebApplicationInfoFactory =
    base::RepeatingCallback<std::unique_ptr<WebApplicationInfo>()>;

enum class ExternalInstallSource;

struct ExternalInstallOptions {
  ExternalInstallOptions(const GURL& install_url,
                         DisplayMode user_display_mode,
                         ExternalInstallSource install_source);
  ~ExternalInstallOptions();
  ExternalInstallOptions(const ExternalInstallOptions& other);
  ExternalInstallOptions(ExternalInstallOptions&& other);
  ExternalInstallOptions& operator=(const ExternalInstallOptions& other);

  bool operator==(const ExternalInstallOptions& other) const;

  GURL install_url;
  DisplayMode user_display_mode;
  ExternalInstallSource install_source;

  // App name to use for placeholder apps or web apps that have no name in
  // their manifest.
  absl::optional<std::string> fallback_app_name;

  // If true, a shortcut is added to the Applications folder on macOS, and Start
  // Menu on Linux and Windows and launcher on Chrome OS. If false, we skip
  // adding a shortcut to desktop as well, regardless of the value of
  // |add_to_desktop|.
  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  bool add_to_applications_menu = true;

  // If true, a shortcut is added to the desktop on Linux and Windows. Has no
  // effect on macOS and Chrome OS.
  bool add_to_desktop = true;

  // If true, a shortcut is added to the "quick launch bar" of the OS: the Shelf
  // for Chrome OS, the Dock for macOS, and the Quick Launch Bar or Taskbar on
  // Windows. Currently this only works on Chrome OS.
  bool add_to_quick_launch_bar = true;

  // If true, the app can be searched for on Chrome OS. Has no effect on other
  // platforms.
  bool add_to_search = true;

  // If true, the app is shown in App Management on Chrome OS. Has no effect on
  // other platforms.
  bool add_to_management = true;

  // Whether the app should be registered to run on OS login.
  // Currently this only works on Windows by adding a shortcut to the
  // Startup Folder.
  // TODO(crbug.com/897302): Enable for other platforms.
  bool run_on_os_login = false;

  // If true, the app icon is displayed on Chrome OS with a blocked logo on
  // top, and the user cannot launch the app. Has no effect on other platforms.
  bool is_disabled = false;

  // Whether the app should be reinstalled even if the user has previously
  // uninstalled it.
  bool override_previous_user_uninstall = false;

  // Whether the app should only be installed if the user is using Chrome for
  // the first time.
  bool only_for_new_users = false;

  // Whether the app should be omitted for new preinstalls. This is the opposite
  // of |only_for_new_users| option. It can be used for partial deprecation of
  // a preinstalled app: do not preinstall the app for new users but leave
  // existing users unaffected.
  bool only_if_previously_preinstalled = false;

  // Which user types this app should be installed for.
  // See apps::DetermineUserType() for relevant string constants.
  std::vector<std::string> user_type_allowlist;

  // Which feature flag should be enabled to install this app. See
  // chrome/browser/web_applications/components/preinstalled_app_install_features.h
  // for available features to gate on.
  absl::optional<std::string> gate_on_feature;

  // Whether this should not be installed for devices that support ARC.
  bool disable_if_arc_supported = false;

  // Whether this should not be installed for tablet devices.
  bool disable_if_tablet_form_factor = false;

  // This must only be used by pre-installed default or system apps that are
  // valid PWAs if loading the real service worker is too costly to verify
  // programmatically.
  bool bypass_service_worker_check = false;

  // When set to true this will fail installation with
  // |kNotValidManifestForWebApp| if the |install_url| doesn't have a manifest
  // that passes basic validity checks. This is ignored when |app_info_factory|
  // is used.
  bool require_manifest = false;

  // Whether the app should be reinstalled even if it is already installed.
  bool force_reinstall = false;

  // Whether we should update the app if the browser's binary milestone number
  // goes from less the milestone specified to greater or equal than the
  // milestone specified. For example, if this value is 89 then we update the
  // app on all browser upgrades from <89 to >=89. The update happens only once.
  absl::optional<int> force_reinstall_for_milestone;

  // Whether we should wait for all app windows being closed before reinstalling
  // the placeholder.
  bool wait_for_windows_closed = false;

  // Whether a placeholder app should be installed if we fail to retrieve the
  // metadata for the app. A placeholder app uses:
  //  - The default Chrome App icon for the icon
  //  - |url| as the start_url
  //  - |url| as the app name (unless fallback_app_name has been specified)
  bool install_placeholder = false;

  // Whether we should try to reinstall the app if there is a placeholder for
  // it.
  bool reinstall_placeholder = false;

  // Optional query parameters to add to the start_url when launching the app.
  absl::optional<std::string> launch_query_params;

  // Whether we should load |service_worker_registration_url| after successful
  // installation to allow the site to install its service worker and set up
  // offline caching.
  bool load_and_await_service_worker_registration = true;

  // The URL to use for service worker registration. This is
  // configurable by sites that wish to be able to track install metrics of the
  // install_url separate from the service worker registration step. Defaults to
  // install_url if unset.
  absl::optional<GURL> service_worker_registration_url;

  // A list of app_ids that the Web App System should attempt to uninstall and
  // replace with this app (e.g maintain shelf pins, app list positions).
  std::vector<AppId> uninstall_and_replace;

  // Additional keywords that will be used by the OS when searching for the app.
  // Only affects Chrome OS.
  std::vector<std::string> additional_search_terms;

  // Determines whether |app_info_factory| is used as a fallback or the primary
  // source of app metadata. If true the |install_url| and
  // |service_worker_registration_url| will not be loaded.
  bool only_use_app_info_factory = false;

  // A factory callback that returns a unique_ptr<WebApplicationInfo> to be used
  // as the app's installation metadata.
  WebApplicationInfoFactory app_info_factory;

  // The type of SystemWebApp, if this app is a System Web App.
  absl::optional<SystemAppType> system_app_type = absl::nullopt;

  // Whether the app was installed by an OEM and should be placed in a special
  // OEM folder in the app launcher. Only used on Chrome OS.
  bool oem_installed = false;
};

std::ostream& operator<<(std::ostream& out,
                         const ExternalInstallOptions& install_options);

InstallManager::InstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_EXTERNAL_INSTALL_OPTIONS_H_
