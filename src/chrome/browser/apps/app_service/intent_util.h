// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/intent_common.mojom-forward.h"
#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"

namespace arc {
class ArcIntentHelperBridge;
class IntentFilter;
}
#endif

namespace apps {
struct ShareTarget;
}

class GURL;
class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class Extension;
}  // namespace extensions

namespace web_app {
class WebApp;
}  // namespace web_app

namespace apps_util {

// Creates a file filter.
apps::mojom::IntentFilterPtr CreateFileFilter(
    const std::vector<std::string>& intent_actions,
    const std::vector<std::string>& mime_types,
    const std::vector<std::string>& file_extensions,
    const std::string& activity_name = "",
    bool include_directories = false);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Create intent filters from `package_name` and `intent_helper_bridge`.
apps::IntentFilters CreateIntentFiltersFromArcBridge(
    const std::string& package_name,
    arc::ArcIntentHelperBridge* intent_helper_bridge);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Create intent filters for `app_id`. The `app_scope` is needed because
// currently the correct app scope is not provided through WebApp API for
// shortcuts.
apps::IntentFilters CreateIntentFiltersForWebApp(
    const web_app::AppId& app_id,
    bool is_note_taking_web_app,
    const GURL& app_scope,
    const apps::ShareTarget* app_share_target,
    const apps::FileHandlers* enabled_file_handlers);

// Create intent filters for |web_app|.
// The |scope| is needed because currently the correct app scope is not
// provided through WebApp API for shortcuts.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
std::vector<apps::mojom::IntentFilterPtr> CreateWebAppIntentFilters(
    const web_app::AppId& app_id,
    bool is_note_taking_web_app,
    const GURL& app_scope,
    const apps::ShareTarget* app_share_target,
    const apps::FileHandlers* enabled_file_handlers);

// Create intent filters for a Chrome app (extension-based) e.g. for
// file_handlers.
apps::IntentFilters CreateIntentFiltersForChromeApp(
    const extensions::Extension* extension);

// Create intent filters for a Chrome app (extension-based) e.g. for
// file_handlers.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
std::vector<apps::mojom::IntentFilterPtr> CreateChromeAppIntentFilters(
    const extensions::Extension* extension);

// Create intent filters for an Extension (is_extension() == true) e.g. for
// file_browser_handlers.
apps::IntentFilters CreateIntentFiltersForExtension(
    const extensions::Extension* extension);

// Create intent filters for an Extension (is_extension() == true) e.g. for
// file_browser_handlers.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
std::vector<apps::mojom::IntentFilterPtr> CreateExtensionIntentFilters(
    const extensions::Extension* extension);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Create an intent struct with filesystem:// type URLs from the file paths and
// mime types of a list of files. This util has to live under chrome/ because it
// uses fileapis and cannot be included in components/.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types);

// Create an intent struct with filesystem:// type URLs from the file paths and
// mime types of a list of files, and the share text and title. This util has to
// live under chrome/ because it uses fileapis and cannot be included in
// components/.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    Profile* profile,
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title);

base::flat_map<std::string, std::string> CreateArcIntentExtras(
    const apps::mojom::IntentPtr& intent);

// Convert between App Service and ARC Intents.
arc::mojom::IntentInfoPtr ConvertAppServiceToArcIntent(
    const apps::mojom::IntentPtr& intent);

// Converts an ARC intent action to an App Service intent action. Returns
// nullptr if |arc_action| is an action which is not supported by App Service.
const char* ConvertArcToAppServiceIntentAction(const std::string& arc_action);

// Convert an apps::mojom::Intent struct to a string to call the LaunchIntent
// interface from arc::mojom::AppInstance. If |intent| has |ui_bypassed|, |url|
// or |data|, returns an empty string as these intents cannot be represented in
// string form.
std::string CreateLaunchIntent(const std::string& package_name,
                               const apps::mojom::IntentPtr& intent);

// Convert between App Service and ARC IntentFilters.
arc::IntentFilter ConvertAppServiceToArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter);
apps::mojom::IntentFilterPtr ConvertArcToAppServiceIntentFilter(
    const arc::IntentFilter& arc_intent_filter);
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Convert App Service Intent to Crosapi Intent.
// |profile| is only needed when the intent contains files, can be filled with
// null otherwise.
// If |profile| is null when converting intent contains files, the files
// fields will not be converted.
// TODO(crbug.com/1253219): Needs manual conversion rather than mojom traits
// because Lacros does not support FileSystemURL as Ash, this method can be
// replaced with mojom traits after migrating the App Service Intent to use the
// file path.
crosapi::mojom::IntentPtr ConvertAppServiceToCrosapiIntent(
    const apps::mojom::IntentPtr& app_service_intent,
    Profile* profile);

// Convert Crosapi Intent to App Service Intent. Note that the converted App
// Service Intent will not contain the files field in lacros-chrome.
// |profile| is only needed when the intent contains files, can be filled with
// null otherwise.
// If |profile| is null when converting intent contains files, the files
// fields will not be converted.
// TODO(crbug.com/1253219): Needs manual conversion rather than mojom traits
// because Lacros does not support FileSystemURL as Ash, this method can be
// replaced with mojom traits after migrating the App Service Intent to use the
// file path.
apps::mojom::IntentPtr ConvertCrosapiToAppServiceIntent(
    const crosapi::mojom::IntentPtr& crosapi_intent,
    Profile* profile);

crosapi::mojom::IntentPtr CreateCrosapiIntentForViewFiles(
    const apps::mojom::FilePathsPtr& file_paths);
#endif
}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_INTENT_UTIL_H_
