// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

// Helper struct for computing the result of fetching account capabilities.
struct FetchCapabilitiesResult {
  FetchCapabilitiesResult() = default;
  ~FetchCapabilitiesResult() = default;

  ChromeIdentityCapabilityResult capability_value;
  signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult fetch_result;
};

// Computes the value of fetching account capabilities.
FetchCapabilitiesResult ComputeFetchCapabilitiesResult(
    NSNumber* capability_value,
    NSError* error) {
  FetchCapabilitiesResult result;
  if (error) {
    result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
    result.fetch_result = signin_metrics::
        FetchAccountCapabilitiesFromSystemLibraryResult::kErrorGeneric;
  } else if (!capability_value) {
    result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
    result.fetch_result =
        signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult::
            kErrorMissingCapability;
  } else {
    int capability_value_int = capability_value.intValue;
    switch (capability_value_int) {
      case static_cast<int>(ChromeIdentityCapabilityResult::kFalse):
      case static_cast<int>(ChromeIdentityCapabilityResult::kTrue):
      case static_cast<int>(ChromeIdentityCapabilityResult::kUnknown):
        result.capability_value =
            static_cast<ChromeIdentityCapabilityResult>(capability_value_int);
        result.fetch_result = signin_metrics::
            FetchAccountCapabilitiesFromSystemLibraryResult::kSuccess;
        break;
      default:
        result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
        result.fetch_result =
            signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult::
                kErrorUnexpectedValue;
    }
  }
  return result;
}

}  // namespace

ChromeIdentityService::ChromeIdentityService() {}

ChromeIdentityService::~ChromeIdentityService() {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceWillBeDestroyed();
}

void ChromeIdentityService::DismissDialogs() {}

bool ChromeIdentityService::HandleSessionOpenURLContexts(UIScene* scene,
                                                         NSSet* URLContexts) {
  return false;
}

void ChromeIdentityService::ApplicationDidDiscardSceneSessions(
    NSSet* scene_sessions) {}

DismissASMViewControllerBlock
ChromeIdentityService::PresentAccountDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentWebAndAppSettingDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

ChromeIdentityInteractionManager*
ChromeIdentityService::CreateChromeIdentityInteractionManager() const {
  NOTREACHED() << "Subclasses must override this";
  return nil;
}

void ChromeIdentityService::IterateOverIdentities(IdentityIteratorCallback) {}

void ChromeIdentityService::ForgetIdentity(ChromeIdentity* identity,
                                           ForgetIdentityCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::string& client_id,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAvatarForIdentity(ChromeIdentity* identity) {
  NOTREACHED();
}

UIImage* ChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  NOTREACHED();
  return nil;
}

void ChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {}

NSString* ChromeIdentityService::GetCachedHostedDomainForIdentity(
    ChromeIdentity* identity) {
  // @gmail.com accounts are end consumer accounts so it is safe to return @""
  // even when SSOProfileSource has a nil profile for |sso_identity|.
  //
  // Note: This is also needed during the sign-in flow as it avoids waiting for
  // the profile of |sso_identity| to be fetched from the server.
  if (gaia::ExtractDomainName(base::SysNSStringToUTF8(identity.userEmail)) ==
      "gmail.com") {
    return @"";
  }
  return nil;
}

void ChromeIdentityService::CanOfferExtendedSyncPromos(
    ChromeIdentity* identity,
    CapabilitiesCallback completion) {
  NSString* canOfferExtendedChromeSyncPromos = [NSString
      stringWithUTF8String:kCanOfferExtendedChromeSyncPromosCapabilityName];
  base::TimeTicks fetch_start = base::TimeTicks::Now();
  FetchCapabilities(
      @[ canOfferExtendedChromeSyncPromos ], identity,
      ^(NSDictionary<NSString*, NSNumber*>* capabilities, NSError* error) {
        base::UmaHistogramTimes(
            "Signin.AccountCapabilities.GetFromSystemLibraryDuration",
            base::TimeTicks::Now() - fetch_start);

        FetchCapabilitiesResult result = ComputeFetchCapabilitiesResult(
            [capabilities objectForKey:canOfferExtendedChromeSyncPromos],
            error);
        base::UmaHistogramEnumeration(
            "Signin.AccountCapabilities.GetFromSystemLibraryResult",
            result.fetch_result);

        if (completion)
          completion(result.capability_value);
      });
}

bool ChromeIdentityService::IsServiceSupported() {
  return false;
}

MDMDeviceStatus ChromeIdentityService::GetMDMDeviceStatus(
    NSDictionary* user_info) {
  return 0;
}

bool ChromeIdentityService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info,
                                                  MDMStatusCallback callback) {
  return false;
}

bool ChromeIdentityService::IsMDMError(ChromeIdentity* identity,
                                       NSError* error) {
  return false;
}

void ChromeIdentityService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeIdentityService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ChromeIdentityService::IsInvalidGrantError(NSDictionary* user_info) {
  return false;
}

void ChromeIdentityService::FetchCapabilities(
    NSArray* capabilities,
    ChromeIdentity* identity,
    ChromeIdentityCapabilitiesFetchCompletionBlock completion) {
  // Implementation provided by subclass.
}

void ChromeIdentityService::FireIdentityListChanged(bool keychainReload) {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged(keychainReload);
}

void ChromeIdentityService::FireAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, user_info);
}

void ChromeIdentityService::FireProfileDidUpdate(ChromeIdentity* identity) {
  for (auto& observer : observer_list_)
    observer.OnProfileUpdate(identity);
}

}  // namespace ios
