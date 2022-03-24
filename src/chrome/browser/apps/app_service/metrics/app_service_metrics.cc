// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void RecordDefaultAppLaunch(apps::DefaultAppName default_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
    case apps::mojom::LaunchSource::kFromTest:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListGrid",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromAppListQuery",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromShelf",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFileManager",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromLink:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromLink",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromOmnibox:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOmnibox",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromChromeInternal",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromKeyboard:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKeyboard",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromOtherApp:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOtherApp",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromMenu:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromMenu",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromInstalledNotification", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromArc:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromArc",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromSharesheet:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromSharesheet",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromReleaseNotesNotification",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFullRestore:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromFullRestore",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromSmartTextContextMenu", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
      base::UmaHistogramEnumeration(
          "Apps.DefaultAppLaunch.FromDiscoverTabNotification",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromManagementApi:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromManagementApi",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromKiosk:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromKiosk",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromNewTabPage:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromNewTabPage",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromIntentUrl:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromIntentUrl",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromOsLogin:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromOsLogin",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromProtocolHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromProtocolHandler",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromUrlHandler:
      base::UmaHistogramEnumeration("Apps.DefaultAppLaunch.FromUrlHandler",
                                    default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromCommandLine:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
      NOTREACHED();
      break;
  }
}

void RecordBuiltInAppLaunch(apps::BuiltInAppName built_in_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      base::UmaHistogramEnumeration("Apps.AppListInternalApp.Activate",
                                    built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Open",
                                    built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
    case apps::mojom::LaunchSource::kFromManagementApi:
    case apps::mojom::LaunchSource::kFromKiosk:
    case apps::mojom::LaunchSource::kFromCommandLine:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
    case apps::mojom::LaunchSource::kFromNewTabPage:
    case apps::mojom::LaunchSource::kFromIntentUrl:
    case apps::mojom::LaunchSource::kFromOsLogin:
    case apps::mojom::LaunchSource::kFromProtocolHandler:
    case apps::mojom::LaunchSource::kFromUrlHandler:
      break;
  }
}

}  // namespace

namespace apps {

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source) {
  if (app_id == web_app::kCursiveAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kCursive, launch_source);
  } else if (app_id == extension_misc::kCalculatorAppId) {
    // Launches of the legacy calculator chrome app.
    RecordDefaultAppLaunch(DefaultAppName::kCalculatorChromeApp, launch_source);
  } else if (app_id == extension_misc::kTextEditorAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kText, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == file_manager::kAudioPlayerAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kAudioPlayer, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kCalculatorAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kCalculator, launch_source);
  } else if (app_id == web_app::kCanvasAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kChromeCanvas, launch_source);
  } else if (app_id == web_app::kCameraAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kCamera, launch_source);
  } else if (app_id == web_app::kHelpAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kHelpApp, launch_source);
  } else if (app_id == web_app::kMediaAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kMediaApp, launch_source);
  } else if (app_id == extension_misc::kChromeAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kChrome, launch_source);
  } else if (app_id == extension_misc::kGoogleDocsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDocs, launch_source);
  } else if (app_id == extension_misc::kGoogleDriveAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDrive, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == arc::kGoogleDuoAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDuo, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kFilesManagerAppId ||
             app_id == file_manager::kFileManagerSwaAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kFiles, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGmailAppId ||
             app_id == arc::kGmailAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kGmail, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGoogleKeepAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kKeep, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kGooglePhotosAppId ||
             app_id == arc::kGooglePhotosAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPhotos, launch_source);
  } else if (app_id == arc::kPlayBooksAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayBooks, launch_source);
  } else if (app_id == arc::kPlayGamesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayGames, launch_source);
  } else if (app_id == arc::kPlayMoviesAppId ||
             app_id == extension_misc::kGooglePlayMoviesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayMovies, launch_source);
  } else if (app_id == arc::kPlayMusicAppId ||
             app_id == extension_misc::kGooglePlayMusicAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayMusic, launch_source);
  } else if (app_id == arc::kPlayStoreAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPlayStore, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kOsSettingsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kSettings, launch_source);
  } else if (app_id == extension_misc::kGoogleSheetsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kSheets, launch_source);
  } else if (app_id == extension_misc::kGoogleSlidesAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kSlides, launch_source);
  } else if (app_id == extensions::kWebStoreAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kWebStore, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == extension_misc::kYoutubeAppId ||
             app_id == arc::kYoutubeAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kYouTube, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kYoutubeMusicAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kYouTubeMusic, launch_source);
  } else if (app_id == web_app::kStadiaAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kStadia, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kScanningAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kScanningApp, launch_source);
  } else if (app_id == web_app::kDiagnosticsAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kDiagnosticsApp, launch_source);
  } else if (app_id == web_app::kPrintManagementAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kPrintManagementApp, launch_source);
  } else if (app_id == web_app::kShortcutCustomizationAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kShortcutCustomizationApp,
                           launch_source);
  } else if (app_id == web_app::kShimlessRMAAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kShimlessRMAApp, launch_source);
  } else if (app_id == ash::kChromeUITrustedProjectorSwaAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kProjector, launch_source);
  } else if (app_id == web_app::kFirmwareUpdateAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kFirmwareUpdateApp, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // Above are default apps; below are built-in apps.

  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    RecordBuiltInAppLaunch(BuiltInAppName::kKeyboardShortcutViewer,
                           launch_source);
  } else if (app_id == ash::kInternalAppIdSettings) {
    RecordBuiltInAppLaunch(BuiltInAppName::kSettings, launch_source);
  } else if (app_id == ash::kInternalAppIdContinueReading) {
    RecordBuiltInAppLaunch(BuiltInAppName::kContinueReading, launch_source);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == plugin_vm::kPluginVmShelfAppId) {
    RecordBuiltInAppLaunch(BuiltInAppName::kPluginVm, launch_source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == web_app::kMockSystemAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kMockSystemApp, launch_source);
  } else if (app_id == web_app::kOsFeedbackAppId) {
    RecordDefaultAppLaunch(DefaultAppName::kOsFeedbackApp, launch_source);
  }
}

void RecordBuiltInAppSearchResult(const std::string& app_id) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kKeyboardShortcutViewer);
  } else if (app_id == ash::kInternalAppIdSettings) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kSettings);
  } else if (app_id == ash::kInternalAppIdContinueReading) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kContinueReading);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (app_id == plugin_vm::kPluginVmShelfAppId) {
    base::UmaHistogramEnumeration("Apps.AppListSearchResultInternalApp.Show",
                                  BuiltInAppName::kPluginVm);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
}

void RecordAppBounce(const apps::AppUpdate& app) {
  base::Time install_time = app.InstallTime();
  base::Time uninstall_time = base::Time::Now();

  DCHECK(uninstall_time >= install_time);

  base::TimeDelta amount_time_installed = uninstall_time - install_time;

  const base::TimeDelta seven_days = base::Days(7);

  if (amount_time_installed < seven_days) {
    base::UmaHistogramBoolean("Apps.Bounced", true);
  } else {
    base::UmaHistogramBoolean("Apps.Bounced", false);
  }
}

void RecordAppsPerNotification(int count) {
  if (count <= 0) {
    return;
  }
  base::UmaHistogramBoolean("ChromeOS.Apps.NumberOfAppsForNotification",
                            (count > 1));
}

}  // namespace apps
