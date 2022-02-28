// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
#define CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/buildflags.h"

namespace ash {
class NewWindowDelegateProvider;
}

namespace chromeos {
class NetworkPortalNotificationController;
}

namespace policy {
class DisplaySettingsHandler;
}

class AccessibilityControllerClient;
class AmbientClientImpl;
class AppListClientImpl;
class AshShellInit;
class AshWebViewFactoryImpl;
class CastConfigControllerMediaRouter;
class DesksTemplatesClient;
class ImeControllerClientImpl;
class InSessionAuthDialogClient;
class LoginScreenClientImpl;
class MediaClientImpl;
class MicrophoneMuteNotificationDelegateImpl;
class MobileDataNotifications;
class NetworkConnectDelegateChromeOS;
class NightLightClient;
class ScreenOrientationDelegateChromeos;
class SessionControllerClientImpl;
class SystemTrayClientImpl;
class TabClusterUIClient;
class TabletModePageBehavior;
class VpnListForwarder;
class WallpaperControllerClientImpl;
class ProjectorAppClientImpl;
class ProjectorClientImpl;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
class ExoParts;
#endif

namespace internal {
class ChromeShelfControllerInitializer;
}

// Browser initialization for Ash UI. Only use this for Ash specific
// intitialization (e.g. initialization of chrome/browser/ui/ash classes).
class ChromeBrowserMainExtraPartsAsh : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAsh();

  ChromeBrowserMainExtraPartsAsh(const ChromeBrowserMainExtraPartsAsh&) =
      delete;
  ChromeBrowserMainExtraPartsAsh& operator=(
      const ChromeBrowserMainExtraPartsAsh&) = delete;

  ~ChromeBrowserMainExtraPartsAsh() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreCreateMainMessageLoop() override;
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

 private:
  class UserProfileLoadedObserver;

  std::unique_ptr<UserProfileLoadedObserver> user_profile_loaded_observer_;

  // Initialized in PreProfileInit in all configs before Shell init:
  std::unique_ptr<NetworkConnectDelegateChromeOS> network_connect_delegate_;
  std::unique_ptr<CastConfigControllerMediaRouter>
      cast_config_controller_media_router_;

  // Initialized in PreProfileInit if ash config != MASH:
  std::unique_ptr<AshShellInit> ash_shell_init_;

  // Initialized in PreProfileInit in all configs after Shell init:
  std::unique_ptr<AccessibilityControllerClient>
      accessibility_controller_client_;
  std::unique_ptr<AppListClientImpl> app_list_client_;
  std::unique_ptr<ash::NewWindowDelegateProvider> new_window_delegate_provider_;
  std::unique_ptr<ImeControllerClientImpl> ime_controller_client_;
  std::unique_ptr<InSessionAuthDialogClient> in_session_auth_dialog_client_;
  std::unique_ptr<ScreenOrientationDelegateChromeos>
      screen_orientation_delegate_;
  std::unique_ptr<SessionControllerClientImpl> session_controller_client_;
  std::unique_ptr<SystemTrayClientImpl> system_tray_client_;
  std::unique_ptr<TabClusterUIClient> tab_cluster_ui_client_;
  std::unique_ptr<TabletModePageBehavior> tablet_mode_page_behavior_;
  std::unique_ptr<VpnListForwarder> vpn_list_forwarder_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
  std::unique_ptr<ProjectorAppClientImpl> projector_app_client_;
  std::unique_ptr<ProjectorClientImpl> projector_client_;
  // TODO(stevenjb): Move NetworkPortalNotificationController to c/b/ui/ash and
  // elim chromeos:: namespace. https://crbug.com/798569.
  std::unique_ptr<chromeos::NetworkPortalNotificationController>
      network_portal_notification_controller_;

  std::unique_ptr<internal::ChromeShelfControllerInitializer>
      chrome_shelf_controller_initializer_;
  std::unique_ptr<DesksTemplatesClient> desks_templates_client_;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  std::unique_ptr<ExoParts> exo_parts_;
#endif

  // Initialized in PostProfileInit in all configs:
  std::unique_ptr<LoginScreenClientImpl> login_screen_client_;
  std::unique_ptr<MediaClientImpl> media_client_;
  std::unique_ptr<MicrophoneMuteNotificationDelegateImpl>
      microphone_mute_notification_delegate_;
  std::unique_ptr<policy::DisplaySettingsHandler> display_settings_handler_;
  std::unique_ptr<AshWebViewFactoryImpl> ash_web_view_factory_;

  // Initialized in PostBrowserStart in all configs:
  std::unique_ptr<MobileDataNotifications> mobile_data_notifications_;
  std::unique_ptr<NightLightClient> night_light_client_;
  std::unique_ptr<AmbientClientImpl> ambient_client_;
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
