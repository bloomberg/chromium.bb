// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserContainerViewController () {
  // Weak reference to content view, so old _contentView can be removed from
  // superview when new one is added.
  __weak UIView* _contentView;
}
@end

@implementation BrowserContainerViewController

- (void)dealloc {
  DCHECK(![_contentView superview] || [_contentView superview] == self.view);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)())completion {
  if (!self.presentedViewController) {
    // TODO(crbug.com/801165): On iOS10, UIDocumentMenuViewController and
    // WKFileUploadPanel somehow combine to call dismiss twice instead of once.
    // The second call would dismiss the BrowserContainerViewController itself,
    // so look for that case and return early.
    //
    // TODO(crbug.com/852367): A similar bug exists on all iOS versions with
    // WKFileUploadPanel and UIDocumentPickerViewController. See also
    // https://crbug.com/811671.
    //
    // Return early whenever this method is invoked but no VC appears to be
    // presented.  These cases will always end up dismissing the
    // BrowserContainerViewController itself, which would put the app into an
    // unresponsive state.
    return;
  }
  [super dismissViewControllerAnimated:animated completion:completion];
}

#pragma mark - Public

- (void)displayContentView:(UIView*)contentView {
  if (_contentView == contentView)
    return;

  DCHECK(![_contentView superview] || [_contentView superview] == self.view);
  [_contentView removeFromSuperview];
  _contentView = contentView;

  if (contentView)
    [self.view addSubview:contentView];
}

@end
