// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "image_copier.h"

#include "base/task/post_task.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Time Period between "Copy Image" is clicked and "Copying..." alert is
// launched.
const int kAlertDelayInMs = 300;
// A speical id indicates that last copy is finished or canceled and next
// copy has not started.
const int kNoActiveCopy = 0;
}

@interface ImageCopier ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// A counter which generates one ID for each call on
// CopyImageAtURL:referrer:webState.
@property(nonatomic) int idGenerator;
// ID of current active copy. A copy is active after
// CopyImageAtURL:referrer:webState is called, and before user cancels the
// copy or the copy finishes.
@property(nonatomic) int activeID;
@end

@implementation ImageCopier

@synthesize alertCoordinator = _alertCoordinator;
@synthesize baseViewController = _baseViewController;
@synthesize idGenerator = _idGenerator;
@synthesize activeID = _activeID;

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    self.idGenerator = 1;
    self.activeID = kNoActiveCopy;
    self.baseViewController = baseViewController;
  }
  return self;
}

- (void)copyImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState {
  // Dismisses current alert.
  [self.alertCoordinator stop];

  __weak ImageCopier* weakSelf = self;

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_CONTENT_COPYIMAGE_ALERT_COPYING)
                         message:nil];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:^() {
                  // Cancels current copy and closes the alert.
                  weakSelf.activeID = kNoActiveCopy;
                  [weakSelf.alertCoordinator stop];
                }
                 style:UIAlertActionStyleCancel];

  // |idGenerator| is initiated to 1 and incremented by 2, so it will always be
  // odd number and won't collides with |kNoActiveCopy| or nil.activeID, which
  // are 0s.
  self.idGenerator += 2;
  self.activeID = self.idGenerator;
  // This var is to be captured by blocks in ImageFetchTabHelper::GetImageData
  // and web::WebThread::PostDelayedTask. When a block is invoked, it uses the
  // captured |callbackID| to check if the copy from where it's started is still
  // alive or has been finished/canceled.
  int callbackID = self.idGenerator;

  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);
  tabHelper->GetImageData(url, referrer, ^(NSData* data) {
    // Checks that the copy has not been canceled.
    if (callbackID == weakSelf.activeID) {
      UIImage* image = [UIImage imageWithData:data];
      if (image) {
        UIPasteboard.generalPasteboard.image = image;
      }
      // Finishes this copy.
      weakSelf.activeID = kNoActiveCopy;
      [weakSelf.alertCoordinator stop];
    }
  });

  // Delays launching alert by |kAlertDelayInMs|.
  web::WebThread::PostDelayedTask(
      web::WebThread::UI, FROM_HERE, base::BindOnce(^{
        // Checks that the copy has not finished yet.
        if (callbackID == weakSelf.activeID) {
          [weakSelf.alertCoordinator start];
        }
      }),
      base::TimeDelta::FromMilliseconds(kAlertDelayInMs));
}

@end
