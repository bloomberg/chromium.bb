// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_pref_util.h"

#include <string>

#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_registry_simple.h"

namespace assistant {
namespace prefs {

const char kAssistantConsentStatus[] =
    "settings.voice_interaction.activity_control.consent_status";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kAssistantConsentStatus,
      static_cast<int>(ash::mojom::ConsentStatus::kUnknown));
}

ash::mojom::ConsentStatus GetConsentStatus(PrefService* pref_service) {
  return static_cast<ash::mojom::ConsentStatus>(
      pref_service->GetInteger(kAssistantConsentStatus));
}

void SetConsentStatus(PrefService* pref_service,
                      ash::mojom::ConsentStatus consent_status) {
  pref_service->SetInteger(kAssistantConsentStatus,
                           static_cast<int>(consent_status));
}

}  // namespace prefs
}  // namespace assistant
