// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_service_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller_model_delegate.h"

class AuthenticationService;
@class GoogleServicesSettingsViewController;
class PrefService;
class SyncSetupService;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync
namespace unified_consent {
class UnifiedConsentService;
}  // namespace unified_consent

// Mediator for the Google services settings.
@interface GoogleServicesSettingsMediator
    : NSObject<GoogleServicesSettingsServiceDelegate,
               GoogleServicesSettingsViewControllerModelDelegate>

// View controller.
@property(nonatomic, weak) id<GoogleServicesSettingsConsumer> consumer;
// Authentication service.
@property(nonatomic, assign) AuthenticationService* authService;

// Designated initializer. All the paramters should not be null.
// |userPrefService|: preference service from the browser state.
// |localPrefService|: preference service from the application context.
- (instancetype)
initWithUserPrefService:(PrefService*)userPrefService
       localPrefService:(PrefService*)localPrefService
            syncService:(browser_sync::ProfileSyncService*)syncService
       syncSetupService:(SyncSetupService*)syncSetupService
  unifiedConsentService:
      (unified_consent::UnifiedConsentService*)unifiedConsentService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_MEDIATOR_H_
