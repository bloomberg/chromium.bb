// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/exo/shell_surface_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {
namespace browser_util {
namespace {

bool g_lacros_enabled_for_test = false;

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const User* user) {
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED_DEPRECATED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

// Returns the vector containing policy data of the device account. In case of
// an error, returns nullopt.
base::Optional<std::vector<uint8_t>> GetDeviceAccountPolicy(
    EnvironmentProvider* environment_provider) {
  if (!user_manager::UserManager::IsInitialized()) {
    LOG(ERROR) << "User not initialized.";
    return base::nullopt;
  }
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    LOG(ERROR) << "No primary user.";
    return base::nullopt;
  }
  std::string policy_data = environment_provider->GetDeviceAccountPolicy();
  return std::vector<uint8_t>(policy_data.begin(), policy_data.end());
}

using InterfaceVersions = base::flat_map<base::Token, uint32_t>;
template <typename T>
void AddVersion(InterfaceVersions* map) {
  (*map)[T::Uuid_] = T::Version_;
}

}  // namespace

// When this feature is enabled, Lacros will be available on stable channel.
const base::Feature kLacrosAllowOnStableChannel{
    "LacrosAllowOnStableChannel", base::FEATURE_ENABLED_BY_DEFAULT};

const char kLacrosStabilitySwitch[] = "lacros-stability";
const char kLacrosStabilityLessStable[] = "less-stable";
const char kLacrosStabilityMoreStable[] = "more-stable";

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
}

base::FilePath GetUserDataDir() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    return base::FilePath(crosapi::kLacrosUserDataPath);
  }
  // For developers on Linux desktop, put the directory under the developer's
  // specified --user-data-dir.
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  return base_path.Append("lacros");
}

bool IsLacrosEnabled() {
  return IsLacrosEnabled(chrome::GetChannel());
}

bool IsLacrosEnabled(Channel channel) {
  // Allows tests to avoid enabling the flag, constructing a fake user manager,
  // creating g_browser_process->local_state(), etc.
  if (g_lacros_enabled_for_test)
    return true;

  if (!base::FeatureList::IsEnabled(chromeos::features::kLacrosSupport)) {
    // Don't log here. It's too spammy.
    return false;
  }

  const User* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    LOG(WARNING)
        << "Unable to get primary user. Lacros-chrome will be disabled";
    return false;
  }

  if (!IsUserTypeAllowed(user)) {
    LOG(WARNING) << "Current user type not allowed to launch lacros-chrome";
    return false;
  }

  // TODO(https://crbug.com/1135494): Remove the free ticket for
  // Channel::UNKNOWN after the policy is set on server side for developers.
  if (channel == Channel::UNKNOWN)
    return true;

  if (!g_browser_process->local_state()->GetBoolean(prefs::kLacrosAllowed)) {
    LOG(WARNING) << "Lacros-chrome is not allowed by policy";
    return false;
  }

  switch (channel) {
    case Channel::UNKNOWN:
    case Channel::CANARY:
    case Channel::DEV:
    case Channel::BETA:
      // Canary/dev/beta builds can use Lacros.
      // Developer builds can use lacros.
      return true;
    case Channel::STABLE:
      return base::FeatureList::IsEnabled(kLacrosAllowOnStableChannel);
  }
}

void SetLacrosEnabledForTest(bool force_enabled) {
  g_lacros_enabled_for_test = force_enabled;
}

bool IsLacrosAllowedToLaunch() {
  return user_manager::UserManager::Get()->GetLoggedInUsers().size() <= 1;
}

bool IsLacrosWindow(const aura::Window* window) {
  const std::string* app_id = exo::GetShellApplicationId(window);
  if (!app_id)
    return false;
  return base::StartsWith(*app_id, kLacrosAppIdPrefix);
}

base::flat_map<base::Token, uint32_t> GetInterfaceVersions() {
  static_assert(
      crosapi::mojom::Crosapi::Version_ == 15,
      "if you add a new crosapi, please add it to the version map here");
  InterfaceVersions versions;
  AddVersion<chromeos::sensors::mojom::SensorHalClient>(&versions);
  AddVersion<crosapi::mojom::AccountManager>(&versions);
  AddVersion<crosapi::mojom::BrowserServiceHost>(&versions);
  AddVersion<crosapi::mojom::CertDatabase>(&versions);
  AddVersion<crosapi::mojom::Clipboard>(&versions);
  AddVersion<crosapi::mojom::Crosapi>(&versions);
  AddVersion<crosapi::mojom::DeviceAttributes>(&versions);
  AddVersion<crosapi::mojom::Feedback>(&versions);
  AddVersion<crosapi::mojom::FileManager>(&versions);
  AddVersion<crosapi::mojom::KeystoreService>(&versions);
  AddVersion<crosapi::mojom::MessageCenter>(&versions);
  AddVersion<crosapi::mojom::MetricsReporting>(&versions);
  AddVersion<crosapi::mojom::Prefs>(&versions);
  AddVersion<crosapi::mojom::ScreenManager>(&versions);
  AddVersion<crosapi::mojom::SnapshotCapturer>(&versions);
  AddVersion<crosapi::mojom::TestController>(&versions);
  AddVersion<crosapi::mojom::UrlHandler>(&versions);
  AddVersion<device::mojom::HidConnection>(&versions);
  AddVersion<device::mojom::HidManager>(&versions);
  AddVersion<media_session::mojom::MediaControllerManager>(&versions);
  AddVersion<media_session::mojom::AudioFocusManager>(&versions);
  AddVersion<media_session::mojom::AudioFocusManagerDebug>(&versions);
  return versions;
}

mojom::BrowserInitParamsPtr GetBrowserInitParams(
    EnvironmentProvider* environment_provider) {
  auto params = mojom::BrowserInitParams::New();
  params->crosapi_version = crosapi::mojom::Crosapi::Version_;
  params->deprecated_ash_metrics_enabled_has_value = true;
  PrefService* local_state = g_browser_process->local_state();
  params->ash_metrics_enabled =
      local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
  params->ash_metrics_managed =
      local_state->IsManagedPreference(metrics::prefs::kMetricsReportingEnabled)
          ? mojom::MetricsReportingManaged::kManaged
          : mojom::MetricsReportingManaged::kNotManaged;

  params->session_type = environment_provider->GetSessionType();
  params->device_mode = environment_provider->GetDeviceMode();
  params->interface_versions = GetInterfaceVersions();
  params->default_paths = environment_provider->GetDefaultPaths();
  params->device_account_gaia_id =
      environment_provider->GetDeviceAccountGaiaId();
  // TODO(crbug.com/1093194): This should be updated to a new value when
  // the long term fix is made in ash-chrome, atomically.
  params->exo_ime_support =
      crosapi::mojom::ExoImeSupport::kConsumedByImeWorkaround;
  params->cros_user_id_hash = chromeos::ProfileHelper::GetUserIdHashFromProfile(
      ProfileManager::GetPrimaryUserProfile());
  params->device_account_policy = GetDeviceAccountPolicy(environment_provider);

  return params;
}

base::ScopedFD CreateStartupData(EnvironmentProvider* environment_provider) {
  auto data = GetBrowserInitParams(environment_provider);
  std::vector<uint8_t> serialized =
      crosapi::mojom::BrowserInitParams::Serialize(&data);

  base::ScopedFD fd(memfd_create("startup_data", 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory backed file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(
          fd.get(), reinterpret_cast<const char*>(serialized.data()),
          serialized.size())) {
    LOG(ERROR) << "Failed to dump the serialized startup data";
    return base::ScopedFD();
  }

  if (lseek(fd.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the FD position";
    return base::ScopedFD();
  }

  return fd;
}

}  // namespace browser_util
}  // namespace crosapi
