// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/sync/driver/trusted_vault_client.h"

@class ChromeIdentity;
@class UIViewController;

namespace ios {

using TrustedVaultSharedKey = std::vector<uint8_t>;
using TrustedVaultSharedKeyList = std::vector<TrustedVaultSharedKey>;

// Abstract class to manage shared keys.
class ChromeTrustedVaultService {
 public:
  ChromeTrustedVaultService();
  ChromeTrustedVaultService(const ChromeTrustedVaultService&) = delete;
  virtual ~ChromeTrustedVaultService();
  ChromeTrustedVaultService& operator=(const ChromeTrustedVaultService&) =
      delete;

  using Observer = syncer::TrustedVaultClient::Observer;

  // Adds/removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Asynchronously fetch the shared keys for |identity|
  // and returns them by calling |callback|.
  virtual void FetchKeys(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(const TrustedVaultSharedKeyList&)> callback) = 0;

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  // TODO(crbug.com/1100278): Make pure.
  virtual void GetIsRecoverabilityDegraded(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(bool)> callback);

  // Presents the trusted vault key reauthentication UI for |identity|.
  // Once the reauth is done and the UI is dismissed, |callback| is called.
  // |callback| is not called if the reauthentication is canceled.
  virtual void Reauthentication(ChromeIdentity* chrome_identity,
                                UIViewController* presentingViewController,
                                void (^callback)(BOOL success,
                                                 NSError* error)) = 0;

  // Cancels the presented trusted vault key reauthentication UI.
  // The reauthentication callback will not be called.
  // If no reauthentication dialog is not present, |callback| is called
  // synchronously.
  virtual void CancelReauthentication(BOOL animated,
                                      void (^callback)(void)) = 0;

 protected:
  // Functions to notify observers.
  void NotifyKeysChanged();
  void NotifyRecoverabilityChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
