// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/settings/remoting_settings_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/settings/setting_option.h"

#include "base/logging.h"

static NSString* const kReusableIdentifierItem =
    @"remotingSettingsViewControllerItem";
static UIColor* kBackgroundColor =
    [UIColor colorWithRed:0.f green:0.67f blue:0.55f alpha:1.f];

@interface RemotingSettingsViewController () {
  MDCAppBar* _appBar;
  NSArray* _sections;
  NSMutableArray* _content;
}
@end

@implementation RemotingSettingsViewController

- (id)init {
  self = [super init];
  if (self) {
    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];

    _appBar.headerViewController.headerView.backgroundColor = kBackgroundColor;
    _appBar.navigationBar.tintColor = [UIColor whiteColor];
    _appBar.navigationBar.titleTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor whiteColor]};
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _appBar.headerViewController.headerView.trackingScrollView =
      self.collectionView;
  [_appBar addSubviewsToParent];

  // TODO(nicholss): X should be an image.
  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithTitle:@"X"
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapClose:)];
  self.navigationItem.leftBarButtonItem = nil;
  self.navigationItem.rightBarButtonItem = closeButton;

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forCellWithReuseIdentifier:kReusableIdentifierItem];

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:UICollectionElementKindSectionHeader];

  self.styler.cellStyle = MDCCollectionViewCellStyleCard;

  _sections = @[
    @"Display options", @"Mouse options", @"Keyboard controls", @"Support"
  ];

  _content = [NSMutableArray array];

  SettingOption* shrinkOption = [[SettingOption alloc] init];
  shrinkOption.title = @"Shrink to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  shrinkOption.subtext = @"Don't change resolution to match window";

  SettingOption* resizeOption = [[SettingOption alloc] init];
  resizeOption.title = @"Resize to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  resizeOption.subtext = @"Update remote resolution to match window";

  [_content addObject:@[ shrinkOption, resizeOption ]];

  SettingOption* trackpadMode = [[SettingOption alloc] init];
  trackpadMode.title = @"Trackpad mode";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  trackpadMode.subtext = @"Screen acts like a trackpad";

  [_content addObject:@[ trackpadMode ]];

  SettingOption* ctrlAltDelOption = [[SettingOption alloc] init];
  ctrlAltDelOption.title = @"Press \"Ctrl+Alt+Del\"";

  SettingOption* printScreenOption = [[SettingOption alloc] init];
  printScreenOption.title = @"Press \"Print Screen\"";

  [_content addObject:@[ ctrlAltDelOption, printScreenOption ]];

  SettingOption* helpCenterOption = [[SettingOption alloc] init];
  helpCenterOption.title = @"Help center";

  SettingOption* faqsOption = [[SettingOption alloc] init];
  faqsOption.title = @"FAQs";

  SettingOption* sendFeedbackOption = [[SettingOption alloc] init];
  sendFeedbackOption.title = @"Send feedback";

  [_content addObject:@[ helpCenterOption, faqsOption, sendFeedbackOption ]];

  DCHECK_EQ(_content.count, _sections.count);
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return (NSInteger)[_content count];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return (NSInteger)[_content[(NSUInteger)section] count];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  cell.textLabel.text =
      ((SettingOption*)
           _content[(NSUInteger)indexPath.section][(NSUInteger)indexPath.item])
          .title;

  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* supplementaryView =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    supplementaryView.textLabel.text = _sections[(NSUInteger)indexPath.section];
    supplementaryView.textLabel.textColor = kBackgroundColor;
  }
  return supplementaryView;
}

#pragma mark - <UICollectionViewDelegateFlowLayout>

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width,
                    MDCCellDefaultOneLineHeight);
}

#pragma mark - Private

- (void)didTapClose:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
