// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Name of histogram to record the number of excess NTP tabs that are removed.
const char kExcessNTPTabsRemoved[] = "IOS.NTP.ExcessRemovedTabCount";
}  // namespace

@interface StartSurfaceSceneAgent ()

// Caches the previous activation level.
@property(nonatomic, assign) SceneActivationLevel previousActivationLevel;

@end

@implementation StartSurfaceSceneAgent

- (id)init {
  self = [super init];
  if (self) {
    self.previousActivationLevel = SceneActivationLevelUnattached;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level != SceneActivationLevelForegroundActive &&
      self.previousActivationLevel == SceneActivationLevelForegroundActive) {
    // TODO(crbug.com/1173160): Consider when to clear the session object since
    // Chrome may be closed without transiting to inactive, e.g. device power
    // off, then the previous session object is staled.
    SetStartSurfaceSessionObjectForSceneState(sceneState);
  }
  if (level == SceneActivationLevelBackground &&
      self.previousActivationLevel > SceneActivationLevelBackground) {
    if (base::FeatureList::IsEnabled(kRemoveExcessNTPs)) {
      // Remove duplicate NTP pages upon background event.
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .mainInterface.browser];
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .incognitoInterface.browser];
    }
  }
  self.previousActivationLevel = level;
}

// Removes duplicate NTP tabs in |browser|'s WebStateList.
- (void)removeExcessNTPsInBrowser:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  web::WebState* activeWebState =
      browser->GetWebStateList()->GetActiveWebState();
  int activeWebStateIndex = webStateList->GetIndexOfWebState(activeWebState);
  NSMutableArray<NSNumber*>* emptyNtpIndices = [[NSMutableArray alloc] init];
  web::WebState* lastNtpWebStatesWithNavHistory = nullptr;
  BOOL keepOneNTP = YES;
  BOOL activeWebStateIsEmptyNTP = NO;
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    if (IsURLNtp(webState->GetVisibleURL())) {
      // Check if there is navigation history for this WebState that is showing
      // the NTP. If there is, then set |keepOneNTP| to NO, indicating that all
      // WebStates in NTPs with no navigation history will get removed.
      // TODO(crbug.com/1291626): Remove IsRealized and check GetItemCount
      // directly on webState.
      if (webState->IsRealized() &&
          webState->GetNavigationManager()->GetItemCount() <= 1) {
        // Keep track if active WebState is showing an NTP and has no navigation
        // history since it may get removed if |keepOneNTP| is NO.
        if (i == activeWebStateIndex) {
          activeWebStateIsEmptyNTP = YES;
        }
        // Insert at the front so that iterating through the array will remove
        // WebStates in descending index order, preventing WebState indices from
        // changing during removal.
        [emptyNtpIndices insertObject:@(i) atIndex:0];
      } else {
        keepOneNTP = NO;
        lastNtpWebStatesWithNavHistory = webState;
      }
    }
  }
  if (keepOneNTP) {
    // If the current active tab may be removed because it is showing the NTP
    // and has no navigation history, then save that tab. Otherwise, keep the
    // first index to save the most recently created tab.
    NSNumber* tabIndexToSave = [emptyNtpIndices firstObject];
    if (activeWebStateIsEmptyNTP &&
        [[emptyNtpIndices lastObject] intValue] != activeWebStateIndex) {
      tabIndexToSave = @(activeWebStateIndex);
    }
    [emptyNtpIndices removeObject:tabIndexToSave];
  }
  UMA_HISTOGRAM_COUNTS_100(kExcessNTPTabsRemoved, [emptyNtpIndices count]);
  // Removal starts from higher indices to ensure tab indices stay fixed
  // throughout removal process.
  for (NSNumber* index in emptyNtpIndices) {
    web::WebState* webState =
        browser->GetWebStateList()->GetWebStateAt([index intValue]);
    DCHECK(IsURLNtp(webState->GetVisibleURL()));
    webStateList->CloseWebStateAt([index intValue],
                                  WebStateList::CLOSE_NO_FLAGS);
  }
  // If the active WebState was removed because it was showing the NTP and had
  // no navigation history, switch to another NTP. This only is needed if there
  // were tabs showing the NTP that had navigation histories. Otherwise, code
  // above already saves the current active WebState right before empty NTPs are
  // removed.
  if (activeWebStateIsEmptyNTP && !keepOneNTP) {
    DCHECK(lastNtpWebStatesWithNavHistory);
    int newActiveIndex =
        webStateList->GetIndexOfWebState(lastNtpWebStatesWithNavHistory);
    webStateList->ActivateWebStateAt(newActiveIndex);
  }
}

@end
