// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"

#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_impl.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {
namespace secure_channel {

// static
NearbyConnector* NearbyConnectorFactory::GetForProfile(Profile* profile) {
  return static_cast<NearbyConnectorImpl*>(
      NearbyConnectorFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
NearbyConnectorFactory* NearbyConnectorFactory::GetInstance() {
  return base::Singleton<NearbyConnectorFactory>::get();
}

NearbyConnectorFactory::NearbyConnectorFactory()
    : BrowserContextKeyedServiceFactory(
          "NearbyConnector",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(nearby::NearbyProcessManagerFactory::GetInstance());
}

NearbyConnectorFactory::~NearbyConnectorFactory() = default;

KeyedService* NearbyConnectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  nearby::NearbyProcessManager* nearby_process_manager =
      nearby::NearbyProcessManagerFactory::GetForProfile(
          Profile::FromBrowserContext(context));

  // If null, control of the Nearby utility process is not supported for this
  // profile.
  if (!nearby_process_manager)
    return nullptr;

  auto nearby_connector =
      std::make_unique<NearbyConnectorImpl>(nearby_process_manager);
  SecureChannelClientProvider::GetInstance()->GetClient()->SetNearbyConnector(
      nearby_connector.get());
  return nearby_connector.release();
}

bool NearbyConnectorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace secure_channel
}  // namespace ash
