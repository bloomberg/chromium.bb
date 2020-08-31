// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_trusted_vault_client.h"

#include "components/signin/public/identity_manager/account_info.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/chrome_trusted_vault_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSTrustedVaultClient::IOSTrustedVaultClient() {}

IOSTrustedVaultClient::~IOSTrustedVaultClient() = default;

std::unique_ptr<IOSTrustedVaultClient::Subscription>
IOSTrustedVaultClient::AddKeysChangedObserver(
    const base::RepeatingClosure& closure) {
  ios::ChromeBrowserProvider* browser_provider =
      ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trusted_vault_service =
      browser_provider->GetChromeTrustedVaultService();
  if (!trusted_vault_service) {
    return nullptr;
  }
  return trusted_vault_service->AddKeysChangedObserver(closure);
}

void IOSTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  ios::ChromeBrowserProvider* browser_provider =
      ios::GetChromeBrowserProvider();
  ios::ChromeIdentityService* identity_service =
      browser_provider->GetChromeIdentityService();
  ChromeIdentity* identity =
      identity_service->GetIdentityWithGaiaID(account_info.gaia);
  ios::ChromeTrustedVaultService* trusted_vault_service =
      browser_provider->GetChromeTrustedVaultService();
  DCHECK(trusted_vault_service);
  trusted_vault_service->FetchKeys(identity, std::move(callback));
}

void IOSTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED();
}

void IOSTrustedVaultClient::RemoveAllStoredKeys() {
  // Not used on iOS.
  NOTREACHED();
}

void IOSTrustedVaultClient::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1019685): needs implementation.
  std::move(callback).Run(false);
}
