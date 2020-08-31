// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/generate_qr_code_activity.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kGenerateQrCodeActivityType =
    @"com.google.chrome.GenerateQrCodeActivityType";
}  // namespace

@interface GenerateQrCodeActivity () {
  GURL _activityURL;
}

@property(nonatomic, weak, readonly) NSString* title;
@property(nonatomic, weak, readonly) id<QRGenerationCommands> handler;

@end

@implementation GenerateQrCodeActivity

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
                    handler:(id<QRGenerationCommands>)handler {
  if (self = [super init]) {
    _activityURL = activityURL;
    _title = title;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kGenerateQrCodeActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_GENERATE_QR_CODE_ACTION);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_generate_qr_code"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return base::FeatureList::IsEnabled(kQRCodeGeneration);
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self.handler
      generateQRCode:[[GenerateQRCodeCommand alloc] initWithURL:_activityURL
                                                          title:self.title]];
  [self activityDidFinish:YES];
}

@end
