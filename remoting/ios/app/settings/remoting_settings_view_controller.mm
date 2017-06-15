// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/settings/remoting_settings_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/settings/setting_option.h"

#include "base/logging.h"

static NSString* const kReusableIdentifierItem = @"remotingSettingsVCItem";

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

    self.view.backgroundColor = RemotingTheme.menuBlueColor;
    _appBar.headerViewController.headerView.backgroundColor =
        RemotingTheme.menuBlueColor;
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

  self.collectionView.backgroundColor = RemotingTheme.menuBlueColor;

  // TODO(nicholss): X should be an image.
  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithImage:RemotingTheme.closeIcon
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

  // TODO(nicholss): All of these strings need to be setup for l18n.
  _sections = @[
    @"Display options", @"Mouse options", @"Keyboard controls", @"Support"
  ];

  _content = [NSMutableArray array];

  SettingOption* shrinkOption = [[SettingOption alloc] init];
  shrinkOption.title = @"Shrink to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  shrinkOption.subtext = @"Don't change resolution to match window";
  shrinkOption.style = OptionCheckbox;
  shrinkOption.checked = NO;

  SettingOption* resizeOption = [[SettingOption alloc] init];
  resizeOption.title = @"Resize to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  resizeOption.subtext = @"Update remote resolution to match window";
  resizeOption.style = OptionCheckbox;
  resizeOption.checked = YES;

  [_content addObject:@[ shrinkOption, resizeOption ]];

  SettingOption* directMode = [[SettingOption alloc] init];
  directMode.title = @"Touch mode";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  directMode.subtext = @"Screen acts like a touch screen";
  directMode.style = OptionSelector;
  directMode.checked = YES;

  SettingOption* trackpadMode = [[SettingOption alloc] init];
  trackpadMode.title = @"Trackpad mode";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  trackpadMode.subtext = @"Screen acts like a trackpad";
  trackpadMode.style = OptionSelector;
  trackpadMode.checked = NO;

  [_content addObject:@[ directMode, trackpadMode ]];

  SettingOption* ctrlAltDelOption = [[SettingOption alloc] init];
  ctrlAltDelOption.title = @"Press \"Ctrl+Alt+Del\"";
  ctrlAltDelOption.style = FlatButton;

  SettingOption* printScreenOption = [[SettingOption alloc] init];
  printScreenOption.title = @"Press \"Print Screen\"";
  printScreenOption.style = FlatButton;

  [_content addObject:@[ ctrlAltDelOption, printScreenOption ]];

  SettingOption* helpCenterOption = [[SettingOption alloc] init];
  helpCenterOption.title = @"Help center";
  helpCenterOption.style = FlatButton;

  SettingOption* faqsOption = [[SettingOption alloc] init];
  faqsOption.title = @"FAQs";
  faqsOption.style = FlatButton;

  SettingOption* sendFeedbackOption = [[SettingOption alloc] init];
  sendFeedbackOption.title = @"Send feedback";
  sendFeedbackOption.style = FlatButton;

  [_content addObject:@[ helpCenterOption, faqsOption, sendFeedbackOption ]];

  DCHECK_EQ(_content.count, _sections.count);

  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
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
  SettingOption* setting = _content[indexPath.section][indexPath.item];
  // TODO(nicholss): There is a bug in MDCCollectionViewTextCell, it has a
  // wrapping UIView that leaves behind a one pixel edge. Filed a bug:
  // https://github.com/material-components/material-components-ios/issues/1519
  MDCCollectionViewTextCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  cell.contentView.backgroundColor = RemotingTheme.menuBlueColor;
  cell.textLabel.text = setting.title;
  cell.textLabel.textColor = [UIColor whiteColor];
  cell.textLabel.numberOfLines = 1;
  cell.detailTextLabel.text = setting.subtext;
  cell.detailTextLabel.textColor = [UIColor whiteColor];
  cell.detailTextLabel.numberOfLines = 1;
  cell.tintColor = RemotingTheme.menuBlueColor;

  switch (setting.style) {
    case OptionCheckbox:
      if (setting.checked) {
        cell.imageView.image = RemotingTheme.checkboxCheckedIcon;
      } else {
        cell.imageView.image = RemotingTheme.checkboxOutlineIcon;
      }
      break;
    case OptionSelector:
      if (setting.checked) {
        cell.imageView.image = RemotingTheme.radioCheckedIcon;
      } else {
        cell.imageView.image = RemotingTheme.radioOutlineIcon;
      }
      break;
    case FlatButton:  // Fall-through.
    default:
      cell.imageView.image = [[UIImage alloc] init];
  }
  return cell;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  SettingOption* setting = _content[indexPath.section][indexPath.item];
  switch (setting.style) {
    case OptionCheckbox:  // Fall-through
    case OptionSelector:
      return MDCCellDefaultTwoLineHeight;
    case FlatButton:  // Fall-through.
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  MDCCollectionViewTextCell* cell = (MDCCollectionViewTextCell*)[collectionView
      cellForItemAtIndexPath:indexPath];

  NSLog(@"Tapped: %@", cell);
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* supplementaryView =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    supplementaryView.contentView.backgroundColor = RemotingTheme.menuBlueColor;
    supplementaryView.textLabel.text = _sections[(NSUInteger)indexPath.section];
    supplementaryView.textLabel.textColor = [UIColor whiteColor];
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
