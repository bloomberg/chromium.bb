// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_chromeos.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/untrusted_eche_app_ui.h"
#include "ash/webui/file_manager/file_manager_untrusted_ui.h"
#include "ash/webui/help_app_ui/help_app_kids_magazine_untrusted_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"
#include "chrome/browser/ash/web_applications/camera_app/camera_app_untrusted_ui_config.h"
#include "chrome/browser/ash/web_applications/crosh_ui.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_untrusted_ui_config.h"
#include "chrome/browser/ash/web_applications/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_annotator_ui_config.h"
#include "chrome/browser/ash/web_applications/projector_app/untrusted_projector_ui_config.h"
#include "chrome/browser/ash/web_applications/terminal_ui.h"

#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterAshChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();
  // Add untrusted `WebUIConfig`s for Ash ChromeOS to the list here.
  map.AddUntrustedWebUIConfig(std::make_unique<CroshUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<TerminalUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<ash::eche_app::UntrustedEcheAppUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<MediaAppGuestUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<ash::HelpAppUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<ash::CameraAppUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<ash::HelpAppKidsMagazineUntrustedUIConfig>());
  if (ash::features::IsProjectorEnabled())
    map.AddUntrustedWebUIConfig(std::make_unique<UntrustedProjectorUIConfig>());
  if (ash::features::IsProjectorAnnotatorEnabled())
    map.AddUntrustedWebUIConfig(
        std::make_unique<UntrustedProjectorAnnotatorUIConfig>());
  if (ash::features::IsFileManagerSwaEnabled()) {
    map.AddUntrustedWebUIConfig(
        std::make_unique<ash::file_manager::FileManagerUntrustedUIConfig>());
  }
  if (base::FeatureList::IsEnabled(ash::features::kOsFeedback)) {
    map.AddUntrustedWebUIConfig(
        std::make_unique<ash::feedback::OsFeedbackUntrustedUIConfig>());
  }
#if !defined(OFFICIAL_BUILD)
  map.AddUntrustedWebUIConfig(
      std::make_unique<ash::UntrustedSampleSystemWebAppUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

void RegisterChromeOSChromeUntrustedWebUIConfigs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterAshChromeUntrustedWebUIConfigs();
#endif
}
