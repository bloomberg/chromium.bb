// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
#define SERVICES_IDENTITY_PUBLIC_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "services/identity/public/cpp/identity_manager.h"

// Implement this protocol and pass your implementation into an
// IdentityManagerObserverBridge object to receive IdentityManager observer
// callbacks in Objective-C.
@protocol IdentityManagerObserverBridgeDelegate<NSObject>

@optional

// These callbacks follow the semantics of the corresponding
// IdentityManager::Observer callbacks. See the comments on
// IdentityManager::Observer in identity_manager.h for the specification of
// these semantics.

- (void)onPrimaryAccountSet:(const AccountInfo&)primaryAccountInfo;
- (void)onPrimaryAccountCleared:(const AccountInfo&)previousPrimaryAccountInfo;
- (void)onRefreshTokenUpdatedForAccount:(const AccountInfo&)accountInfo
                                  valid:(BOOL)isValid;
- (void)onRefreshTokenRemovedForAccount:(const std::string&)accountId;
- (void)onRefreshTokensLoaded;
- (void)onAccountsInCookieUpdated:(const std::vector<AccountInfo>&)accounts;
- (void)onStartBatchOfRefreshTokenStateChanges;
- (void)onEndBatchOfRefreshTokenStateChanges;

@end

namespace identity {

// Bridge class that listens for |IdentityManager| notifications and
// passes them to its Objective-C delegate.
class IdentityManagerObserverBridge : public IdentityManager::Observer {
 public:
  IdentityManagerObserverBridge(
      IdentityManager* identity_manager,
      id<IdentityManagerObserverBridgeDelegate> delegate);
  ~IdentityManagerObserverBridge() override;

  // IdentityManager::Observer.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const std::vector<AccountInfo>& accounts) override;
  void OnStartBatchOfRefreshTokenStateChanges() override;
  void OnEndBatchOfRefreshTokenStateChanges() override;

 private:
  // Identity manager to observe.
  IdentityManager* identity_manager_;
  // Delegate to call.
  __weak id<IdentityManagerObserverBridgeDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerObserverBridge);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_OBJC_IDENTITY_MANAGER_OBSERVER_BRIDGE_H_
