// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_pref_state_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

NetworkPrefStateObserver::NetworkPrefStateObserver() {
  // Initialize NetworkHandler with device prefs only.
  InitializeNetworkPrefServices(nullptr /* profile */);

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                              content::NotificationService::AllSources());
}

NetworkPrefStateObserver::~NetworkPrefStateObserver() {
  NetworkHandler::Get()->ShutdownPrefServices();
}

void NetworkPrefStateObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED, type);
  Profile* profile = content::Details<Profile>(details).ptr();
  DCHECK(profile);

  // Reinitialize the NetworkHandler's pref service when the primary user logs
  // in. Other profiles are ignored because only the primary user's network
  // configuration is used on Chrome OS.
  if (ProfileHelper::IsPrimaryProfile(profile)) {
    InitializeNetworkPrefServices(profile);
    notification_registrar_.RemoveAll();

    auto* wifi_sync_service =
        WifiConfigurationSyncServiceFactory::GetForProfile(profile,
                                                           /*create=*/false);
    if (wifi_sync_service) {
      wifi_sync_service->SetNetworkMetadataStore(
          NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
    }
  }
}

void NetworkPrefStateObserver::InitializeNetworkPrefServices(Profile* profile) {
  DCHECK(g_browser_process);
  NetworkHandler::Get()->InitializePrefServices(
      profile ? profile->GetPrefs() : nullptr,
      g_browser_process->local_state());
}

}  // namespace chromeos
