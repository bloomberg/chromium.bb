// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TranslateInfobarMetricsRecorder

+ (void)recordBannerEvent:(MobileMessagesTranslateBannerEvent)bannerEvent {
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Banner.Event",
                            bannerEvent);
}

+ (void)recordModalEvent:(MobileMessagesTranslateModalEvent)modalEvent {
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Modal.Event",
                            modalEvent);
}

+ (void)recordModalPresent:(MobileMessagesTranslateModalPresent)presentEvent {
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Translate.Modal.Present",
                            presentEvent);
}

+ (void)recordUnusedLegacyInfobarScreenDuration:(NSTimeInterval)duration {
  base::TimeDelta timeDelta = base::TimeDelta::FromSecondsD(duration);
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_MEDIUM_TIMES("Mobile.Legacy.Translate.Unused.Duration",
                             timeDelta);
}

+ (void)recordUnusedInfobar {
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_COUNTS_10M("Mobile.Translate.Unused.Count", 1);
}

+ (void)recordLegacyInfobarToggleDelay:(NSTimeInterval)delay {
  base::TimeDelta timeDelta = base::TimeDelta::FromSecondsD(delay);
  // TODO(crbug.com/1025440): Use function version of macros.
  UMA_HISTOGRAM_MEDIUM_TIMES("Mobile.Legacy.Translate.Toggle.Delay", timeDelta);
}

@end
