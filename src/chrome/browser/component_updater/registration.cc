// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/registration.h"

#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/crowd_deny_component_installer.h"
#include "chrome/browser/component_updater/file_type_policies_component_installer.h"
#include "chrome/browser/component_updater/games_component_installer.h"
#include "chrome/browser/component_updater/mei_preload_component_installer.h"
#include "chrome/browser/component_updater/optimization_hints_component_installer.h"
#include "chrome/browser/component_updater/origin_trials_component_installer.h"
#include "chrome/browser/component_updater/safety_tips_component_installer.h"
#include "chrome/browser/component_updater/ssl_error_assistant_component_installer.h"
#include "chrome/browser/component_updater/sth_set_component_remover.h"
#include "chrome/browser/component_updater/subresource_filter_component_installer.h"
#include "chrome/browser/component_updater/tls_deprecation_config_component_installer.h"
#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_remover.h"
#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#include "components/nacl/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/component_updater/third_party_module_list_component_installer_win.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#else
#include "chrome/browser/component_updater/recovery_component_installer.h"
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
#include "chrome/browser/component_updater/intervention_policy_database_component_installer.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/enterprise/connectors/service_providers.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/component_updater/smart_dim_component_installer.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/component_updater/pepper_flash_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#endif

namespace component_updater {

void RegisterComponentsForUpdate(bool is_off_the_record_profile,
                                 PrefService* profile_prefs) {
  auto* const cus = g_browser_process->component_updater();

#if defined(OS_WIN)
  RegisterRecoveryImprovedComponent(cus, g_browser_process->local_state());
#else
  // TODO(crbug.com/687231): Implement the Improved component on Mac, etc.
  RegisterRecoveryComponent(cus, g_browser_process->local_state());
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PLUGINS)
  RegisterPepperFlashComponent(cus);
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  RegisterWidevineCdmComponent(cus);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if BUILDFLAG(ENABLE_NACL) && !defined(OS_ANDROID)
#if defined(OS_CHROMEOS)
  // PNaCl on Chrome OS is on rootfs and there is no need to download it. But
  // Chrome4ChromeOS on Linux doesn't contain PNaCl so enable component
  // installer when running on Linux. See crbug.com/422121 for more details.
  if (!base::SysInfo::IsRunningOnChromeOS())
#endif  // defined(OS_CHROMEOS)
    RegisterPnaclComponent(cus);
#endif  // BUILDFLAG(ENABLE_NACL) && !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  component_updater::SupervisedUserWhitelistInstaller* whitelist_installer =
      g_browser_process->supervised_user_whitelist_installer();
  whitelist_installer->RegisterComponents();
#endif

  RegisterSubresourceFilterComponent(cus);
  RegisterOnDeviceHeadSuggestComponent(
      cus, g_browser_process->GetApplicationLocale());
  RegisterOptimizationHintsComponent(cus, is_off_the_record_profile,
                                     profile_prefs);

  base::FilePath path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &path)) {
    // The CRLSet component previously resided in a different location: delete
    // the old file.
    component_updater::DeleteLegacyCRLSet(path);

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    // Clean up previous STH sets that may have been installed. This is not
    // done for:
    // Android: Because STH sets were never used
    // Chrome OS: On Chrome OS, this cleanup is delayed until user login.
    component_updater::DeleteLegacySTHSet(path);
#endif

#if !defined(OS_CHROMEOS)
    // CRLSetFetcher attempts to load a CRL set from either the local disk or
    // network.
    // For Chrome OS this registration is delayed until user login.
    component_updater::RegisterCRLSetComponent(cus, path);
#endif  // !defined(OS_CHROMEOS)

    RegisterOriginTrialsComponent(cus, path);

    RegisterFileTypePoliciesComponent(cus, path);

    RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(cus, path);

    RegisterSSLErrorAssistantComponent(cus, path);
  }

  RegisterMediaEngagementPreloadComponent(cus, base::OnceClosure());

#if defined(OS_WIN)
  // SwReporter is only needed for official builds.  However, to enable testing
  // on chromium build bots, it is always registered here and
  // RegisterSwReporterComponent() has support for running only in official
  // builds or tests.
  RegisterSwReporterComponent(cus);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  RegisterThirdPartyModuleListComponent(cus);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
  RegisterInterventionPolicyDatabaseComponent(
      cus, g_browser_process->GetTabManager()->intervention_policy_database());
#endif

#if BUILDFLAG(ENABLE_VR)
  if (component_updater::ShouldRegisterVrAssetsComponentOnStartup()) {
    component_updater::RegisterVrAssetsComponent(cus);
  }
#endif

  RegisterSafetyTipsComponent(cus, path);
  RegisterCrowdDenyComponent(cus, path);
  RegisterTLSDeprecationConfigComponent(cus, path);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_ANDROID)
  component_updater::RegisterGamesComponent(cus, profile_prefs);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled))
    component_updater::RegisterSODAComponent(cus, profile_prefs,
                                             base::OnceClosure());
#endif

#if defined(OS_CHROMEOS)
  RegisterSmartDimComponent(cus);
#endif  // !defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
  enterprise_connectors::RegisterServiceProvidersComponent(cus);
#endif
}

}  // namespace component_updater
