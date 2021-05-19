// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

NSTimeInterval GetTimeSinceMostRecentTabWasOpenForSceneState(
    SceneState* sceneState) {
  if (!IsStartSurfaceEnabled()) {
    return 0;
  }
  NSDate* timestamp = (NSDate*)[sceneState
      sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime];

  if (timestamp == nil || [[NSDate date] timeIntervalSinceDate:timestamp] <
                              GetReturnToStartSurfaceDuration()) {
    return 0;
  }
  return [[NSDate date] timeIntervalSinceDate:timestamp];
}

bool ShouldShowStartSurfaceForSceneState(SceneState* sceneState) {
  if (!IsStartSurfaceEnabled()) {
    return NO;
  }

  NSDate* timestamp = (NSDate*)[sceneState
      sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime];
  if (timestamp == nil || [[NSDate date] timeIntervalSinceDate:timestamp] <
                              GetReturnToStartSurfaceDuration()) {
    return NO;
  }

  if (sceneState.presentingFirstRunUI || sceneState.presentingModalOverlay ||
      sceneState.startupHadExternalIntent || sceneState.pendingUserActivity ||
      sceneState.incognitoContentVisible) {
    return NO;
  }

  return YES;
}

NSString* GetRecentTabTileTimeLabelForSceneState(SceneState* sceneState) {
  NSTimeInterval timeSinceOpen =
      GetTimeSinceMostRecentTabWasOpenForSceneState(sceneState);
  if (timeSinceOpen == 0) {
    return @"";
  }
  NSInteger time = (NSInteger)timeSinceOpen / 3600;
  NSString* timeString = [NSString
      stringWithFormat:@"%@",
                       base::SysUTF16ToNSString(base::FormatNumber(time))];
  NSString* timeLabel =
      l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_HOURS,
                              base::SysNSStringToUTF16(timeString));
  if (time > 24) {
    // If it has been at least a day since the most recent tab was opened,
    // then show days since instead of hours.
    time = time / 24;
    timeString = [NSString
        stringWithFormat:@"%@",
                         base::SysUTF16ToNSString(base::FormatNumber(time))];
    timeLabel = l10n_util::GetNSStringF(IDS_IOS_RETURN_TO_RECENT_TAB_TIME_DAYS,
                                        base::SysNSStringToUTF16(timeString));
  }
  return [NSString stringWithFormat:@" · %@", timeLabel];
}

void SetStartSurfaceSessionObjectForSceneState(SceneState* sceneState) {
  if (!IsStartSurfaceEnabled()) {
    return;
  }

  [sceneState setSessionObject:[NSDate date]
                        forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
}
