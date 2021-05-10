// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/image_util/image_saver.h"

#import <Photos/Photos.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Kill switch guarding a workaround for TCC violations before iOS14.  In case
// iOS14 starts triggering violations too.  See crbug.com/1159431 for details.
const base::Feature kPhotoLibrarySaveImage{"PhotoLibrarySaveImage",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

@interface ImageSaver ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@property(nonatomic, readonly) Browser* browser;
@end

@implementation ImageSaver

@synthesize alertCoordinator = _alertCoordinator;
@synthesize baseViewController = _baseViewController;
@synthesize browser = _browser;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _browser = browser;
  }
  return self;
}

- (void)saveImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState {
  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);

  __weak ImageSaver* weakSelf = self;
  tabHelper->GetImageData(url, referrer, ^(NSData* data) {
    ImageSaver* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    if (data.length == 0) {
      [strongSelf displayPrivacyErrorAlertOnMainQueue:
                      l10n_util::GetNSString(
                          IDS_IOS_SAVE_IMAGE_NO_INTERNET_CONNECTION)];
      return;
    }

    // Use -imageWithData to validate |data|, but continue to pass the raw
    // |data| to -savePhoto to ensure no data loss occurs.
    UIImage* savedImage = [UIImage imageWithData:data];
    if (!savedImage) {
      [strongSelf
          displayPrivacyErrorAlertOnMainQueue:l10n_util::GetNSString(
                                                  IDS_IOS_SAVE_IMAGE_ERROR)];
      return;
    }

    if (base::FeatureList::IsEnabled(kPhotoLibrarySaveImage) &&
        base::ios::IsRunningOnIOS14OrLater()) {
      // Dump |data| into the photo library. Requires the usage of
      // NSPhotoLibraryAddUsageDescription.
      [[PHPhotoLibrary sharedPhotoLibrary]
          performChanges:^{
            PHAssetResourceCreationOptions* options =
                [[PHAssetResourceCreationOptions alloc] init];
            [[PHAssetCreationRequest creationRequestForAsset]
                addResourceWithType:PHAssetResourceTypePhoto
                               data:data
                            options:options];
          }
          completionHandler:^(BOOL success, NSError* error) {
            [weakSelf image:savedImage
                didFinishSavingWithError:error
                             contextInfo:nil];
          }];
    } else {
      // Fallback for pre-iOS14.
      UIImageWriteToSavedPhotosAlbum(
          savedImage, weakSelf,
          @selector(image:didFinishSavingWithError:contextInfo:), nullptr);
    }
  });
}

// Called when Chrome has been denied access to add photos or videos and the
// user can change it.
// Shows a privacy alert on the main queue, allowing the user to go to Chrome's
// settings. Dismiss previous alert if it has not been dismissed yet.
- (void)displayImageErrorAlertWithSettingsOnMainQueue {
  NSURL* settingURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  BOOL canGoToSetting =
      [[UIApplication sharedApplication] canOpenURL:settingURL];
  if (canGoToSetting) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self displayImageErrorAlertWithSettings:settingURL];
    });
  } else {
    [self displayPrivacyErrorAlertOnMainQueue:
              l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE)];
  }
}

// Shows a privacy alert allowing the user to go to Chrome's settings. Dismiss
// previous alert if it has not been dismissed yet.
- (void)displayImageErrorAlertWithSettings:(NSURL*)settingURL {
  // Dismiss current alert.
  [_alertCoordinator stop];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE_GO_TO_SETTINGS);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_browser
                           title:title
                         message:message];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [_alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_GO_TO_SETTINGS)
                action:^{
                  [[UIApplication sharedApplication] openURL:settingURL
                                                     options:@{}
                                           completionHandler:nil];
                }
                 style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

// Called when Chrome has been denied access to the photos or videos and the
// user cannot change it.
// Shows a privacy alert on the main queue, with errorContent as the message.
// Dismisses previous alert if it has not been dismissed yet.
- (void)displayPrivacyErrorAlertOnMainQueue:(NSString*)errorContent {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
    // Dismiss current alert.
    [self.alertCoordinator stop];

    self.alertCoordinator = [[AlertCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:_browser
                             title:title
                           message:errorContent];
    [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                     action:nil
                                      style:UIAlertActionStyleDefault];
    [self.alertCoordinator start];
  });
}

// Called after the system attempts to write the image to the saved photos
// album.
- (void)image:(UIImage*)image
    didFinishSavingWithError:(NSError*)error
                 contextInfo:(void*)contextInfo {
  // Was there an error?
  if (error) {
    // Saving photo failed, likely due to a permissions issue.
    // This code may be execute outside of the main thread. Make sure to display
    // the error on the main thread.
    [self displayImageErrorAlertWithSettingsOnMainQueue];
  } else {
    // TODO(crbug.com/797277): Provide a way for the user to easily reach the
    // photos app.
  }
}

@end
