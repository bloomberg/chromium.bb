// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_keys/extension_key_permissions_service.h"

#include <memory>

#include "base/logging.h"
#include "chrome/browser/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/platform_keys/extension_key_permissions_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension_id.h"

namespace chromeos {
namespace platform_keys {

namespace {

void OnGotExtensionValue(GetExtensionKeyPermissionsServiceCallback callback,
                         content::BrowserContext* context,
                         extensions::ExtensionId extension_id,
                         std::unique_ptr<base::Value> value) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    std::move(callback).Run(/*extension_key_permissions_service=*/nullptr);
    return;
  }

  std::move(callback).Run(std::make_unique<ExtensionKeyPermissionsService>(
      extension_id, extensions::ExtensionSystem::Get(profile)->state_store(),
      std::move(value), profile->GetProfilePolicyConnector()->policy_service(),
      context));
}

}  // namespace

// static
void ExtensionKeyPermissionsServiceFactory::GetForBrowserContextAndExtension(
    GetExtensionKeyPermissionsServiceCallback callback,
    content::BrowserContext* context,
    extensions::ExtensionId extension_id) {
  DCHECK(context);

  extensions::StateStore* const state_store =
      extensions::ExtensionSystem::Get(context)->state_store();

  // Must not be nullptr since KeyPermissionsServiceFactory depends on
  // ExtensionSystemFactory.
  DCHECK(state_store);

  state_store->GetExtensionValue(
      extension_id, kStateStorePlatformKeys,
      base::BindOnce(OnGotExtensionValue, std::move(callback), context,
                     extension_id));
}

ExtensionKeyPermissionsServiceFactory::ExtensionKeyPermissionsServiceFactory() =
    default;
ExtensionKeyPermissionsServiceFactory::
    ~ExtensionKeyPermissionsServiceFactory() = default;

}  // namespace platform_keys
}  // namespace chromeos
