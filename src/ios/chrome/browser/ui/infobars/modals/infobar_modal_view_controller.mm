// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarModalViewController ()

@property(strong, nonatomic) id<InfobarModalDelegate> infobarModalDelegate;

@end

@implementation InfobarModalViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarModalDelegate>)infobarModalDelegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _infobarModalDelegate = infobarModalDelegate;
  }
  return self;
}

#pragma mark - View Lifecycle

// TODO(crbug.com/1372916): PLACEHOLDER UI for the modal ViewController.
- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor lightGrayColor];

  UIButton* dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [dismissButton setTitle:@"Dismiss" forState:UIControlStateNormal];
  [dismissButton addTarget:self.infobarModalDelegate
                    action:@selector(dismissInfobarModal:)
          forControlEvents:UIControlEventTouchUpInside];
  dismissButton.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:dismissButton];

  [NSLayoutConstraint activateConstraints:@[
    [dismissButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [dismissButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [dismissButton.widthAnchor constraintEqualToConstant:100]
  ]];
}

@end
