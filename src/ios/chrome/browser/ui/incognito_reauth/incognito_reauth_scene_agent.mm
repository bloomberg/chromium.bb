// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"

#include "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_util.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoReauthObserverList
    : CRBProtocolObservers <IncognitoReauthObserver>
@end
@implementation IncognitoReauthObserverList
@end

#pragma mark - IncognitoReauthSceneAgent

@interface IncognitoReauthSceneAgent ()

// Whether the window had incognito content (e.g. at least one open tab) upon
// backgrounding.
@property(nonatomic, assign) BOOL windowHadIncognitoContentWhenBackgrounded;

// Tracks wether the user authenticated for incognito since last launch.
@property(nonatomic, assign) BOOL authenticatedSinceLastForeground;

// Container for observers.
@property(nonatomic, strong) IncognitoReauthObserverList* observers;

@end

@implementation IncognitoReauthSceneAgent

#pragma mark - class public

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(prefs::kIncognitoAuthenticationSetting, false);
}

#pragma mark - public

- (instancetype)initWithReauthModule:
    (id<ReauthenticationProtocol>)reauthModule {
  self = [super init];
  if (self) {
    DCHECK(reauthModule);
    _reauthModule = reauthModule;
    _observers = [IncognitoReauthObserverList
        observersWithProtocol:@protocol(IncognitoReauthObserver)];
  }
  return self;
}

- (BOOL)isAuthenticationRequired {
  return [self featureEnabled] &&
         self.windowHadIncognitoContentWhenBackgrounded &&
         !self.authenticatedSinceLastForeground;
}

- (void)authenticateIncognitoContent {
  [self authenticateIncognitoContentWithCompletionBlock:nil];
}

- (void)authenticateIncognitoContentWithCompletionBlock:
    (void (^)(BOOL success))completion {
  DCHECK(self.reauthModule);

  if (!self.isAuthenticationRequired) {
    [self notifyObservers];
    return;
  }

  base::RecordAction(base::UserMetricsAction(
      "MobileIncognitoBiometricAuthenticationRequested"));

  NSString* authReason = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_SYSTEM_DIALOG_REASON,
      base::SysNSStringToUTF16(biometricAuthenticationTypeString()));

  __weak IncognitoReauthSceneAgent* weakSelf = self;
  void (^completionHandler)(ReauthenticationResult) =
      ^(ReauthenticationResult result) {
        BOOL success = (result == ReauthenticationResult::kSuccess);
        base::UmaHistogramBoolean(
            "IOS.Incognito.BiometricReauthAttemptSuccessful", success);

        weakSelf.authenticatedSinceLastForeground = success;
        if (completion) {
          completion(success);
        }
      };
  [self.reauthModule attemptReauthWithLocalizedReason:authReason
                                 canReusePreviousAuth:false
                                              handler:completionHandler];
}

- (void)addObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers removeObserver:observer];
}

#pragma mark properties

- (void)setAuthenticatedSinceLastForeground:(BOOL)authenticated {
  _authenticatedSinceLastForeground = authenticated;
  if (self.featureEnabled) {
    [self notifyObservers];
  }
}

- (void)updateWindowHasIncognitoContent:(SceneState*)sceneState {
  BOOL hasIncognitoContent = YES;
  if (sceneState.interfaceProvider.hasIncognitoInterface) {
    hasIncognitoContent =
        sceneState.interfaceProvider.incognitoInterface.browser
            ->GetWebStateList()
            ->count() > 0;
    // If there is no tabs, act as if the user authenticated since last
    // foreground to avoid issue with multiwindows.
    if (!hasIncognitoContent)
      self.authenticatedSinceLastForeground = YES;
  }

  self.windowHadIncognitoContentWhenBackgrounded = hasIncognitoContent;

  if (self.featureEnabled) {
    [self notifyObservers];
  }
}

- (void)setWindowHadIncognitoContentWhenBackgrounded:(BOOL)hadIncognitoContent {
  if (_windowHadIncognitoContentWhenBackgrounded == hadIncognitoContent) {
    return;
  }
  _windowHadIncognitoContentWhenBackgrounded = hadIncognitoContent;
  if (self.featureEnabled) {
    [self notifyObservers];
  }
}

- (void)notifyObservers {
  DCHECK(self.featureEnabled);
  [self.observers reauthAgent:self
      didUpdateAuthenticationRequirement:self.isAuthenticationRequired];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    [self updateWindowHasIncognitoContent:sceneState];
    self.authenticatedSinceLastForeground = NO;
  } else if (level >= SceneActivationLevelForegroundInactive) {
    [self updateWindowHasIncognitoContent:sceneState];
    [self logEnabledHistogramOnce];
    // Close media presentations when the app is foregrounded rather than
    // backgrounded to avoid freezes.
    [self closeMediaPresentations];
  }
}

#pragma mark - private

// Log authentication setting histogram to determine the feature usage.
// This is done once per app launch.
// Since this agent is created per-scene, guard it with dispatch_once.
- (void)logEnabledHistogramOnce {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    DCHECK(self.localState)
        << "Local state is not yet available when trying to log "
           "IOS.Incognito.BiometricAuthEnabled. This code is called too "
           "soon.";
    BOOL settingEnabled =
        self.localState &&
        self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
    base::UmaHistogramBoolean("IOS.Incognito.BiometricAuthEnabled",
                              settingEnabled);
  });
}

- (PrefService*)localState {
  if (!_localState) {
    if (!GetApplicationContext()) {
      // This is called before application context was initialized.
      return nil;
    }
    _localState = GetApplicationContext()->GetLocalState();
  }
  return _localState;
}

// Convenience method to check the pref associated with the reauth setting and
// the feature flag.
- (BOOL)featureEnabled {
  return self.localState &&
         self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
}

// Closes the media presentations to avoid having the fullscreen video on top of
// the blocker.
- (void)closeMediaPresentations {
  if (![self featureEnabled])
    return;

  Browser* browser =
      self.sceneState.interfaceProvider.incognitoInterface.browser;
  if (browser) {
    if (browser->GetWebStateList() &&
        browser->GetWebStateList()->GetActiveWebState()) {
      browser->GetWebStateList()
          ->GetActiveWebState()
          ->CloseMediaPresentations();
    }
  }
}

@end
