// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#import <Foundation/Foundation.h>

#include "testing/gmock/include/gmock/gmock.h"

@class FakeChromeIdentityInteractionManager;

namespace ios {

// A fake ChromeIdentityService used for testing.
class FakeChromeIdentityService : public ChromeIdentityService {
 public:
  FakeChromeIdentityService();
  virtual ~FakeChromeIdentityService();

  // Convenience method that returns the instance of
  // |FakeChromeIdentityService| from the ChromeBrowserProvider.
  static FakeChromeIdentityService* GetInstanceFromChromeProvider();

  // ChromeIdentityService implementation.
  DismissASMViewControllerBlock PresentAccountDetailsController(
      ChromeIdentity* identity,
      UIViewController* viewController,
      BOOL animated) override;
  ChromeIdentityInteractionManager* CreateChromeIdentityInteractionManager()
      const override;
  FakeChromeIdentityInteractionManager*
  CreateFakeChromeIdentityInteractionManager() const;
  void IterateOverIdentities(IdentityIteratorCallback callback) override;
  void ForgetIdentity(ChromeIdentity* identity,
                      ForgetIdentityCallback callback) override;
  void GetAccessToken(ChromeIdentity* identity,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      ios::AccessTokenCallback callback) override;
  void GetAvatarForIdentity(ChromeIdentity* identity) override;
  UIImage* GetCachedAvatarForIdentity(ChromeIdentity* identity) override;
  void GetHostedDomainForIdentity(ChromeIdentity* identity,
                                  GetHostedDomainCallback callback) override;
  bool IsServiceSupported() override;
  NSString* GetCachedHostedDomainForIdentity(ChromeIdentity* identity) override;

  MOCK_METHOD1(GetMDMDeviceStatus,
               ios::MDMDeviceStatus(NSDictionary* user_info));

  MOCK_METHOD3(HandleMDMNotification,
               bool(ChromeIdentity* identity,
                    NSDictionary* user_info,
                    ios::MDMStatusCallback callback));

  // Simulates |identity| removed from another Google app.
  void SimulateForgetIdentityFromOtherApp(ChromeIdentity* identity);

  // Simulates reloading the identities from the keychain by SSOAuth.
  void FireChromeIdentityReload();

  // Sets up the mock methods for integration tests.
  void SetUpForIntegrationTests();

  // Adds the managed identities given their name.
  void AddManagedIdentities(NSArray* identitiesName);

  // Adds the identities given their name.
  void AddIdentities(NSArray* identitiesNames);

  // Adds |identity| to the available identities. No-op if the identity
  // is already added.
  void AddIdentity(ChromeIdentity* identity);

  // When set to true, call to GetAccessToken() fakes a MDM error.
  void SetFakeMDMError(bool fakeMDMError);

  // Adds a mapping from the |identity| to the capability name -> capability
  // result value used when calling FetchCapabilities.
  // Assumes the |identity| has been added to the available identities.
  void SetCapabilities(ChromeIdentity* identity, NSDictionary* capabilities);

  // Waits until all asynchronous callbacks have been completed by the service.
  // Returns true on successful completion.
  bool WaitForServiceCallbacksToComplete();

  // Triggers an update notification for |identity|.
  void TriggerIdentityUpdateNotification(ChromeIdentity* identity);

 protected:
  void FetchCapabilities(
      NSArray* capabilities,
      ChromeIdentity* identity,
      ChromeIdentityCapabilitiesFetchCompletionBlock completion);

 private:
  NSMutableArray<ChromeIdentity*>* identities_;
  NSMutableDictionary<NSString*, NSDictionary*>* capabilitiesByIdentity_;

  // If true, call to GetAccessToken() fakes a MDM error.
  bool _fakeMDMError;

  int _pendingCallback;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_H_
