// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/settings/remoting_settings_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/settings/setting_option.h"
#import "remoting/ios/app/view_utils.h"

#include "base/logging.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kReusableIdentifierItem = @"remotingSettingsVCItem";
static NSString* const kFeedbackContext = @"InSessionFeedbackContext";

@interface RemotingSettingsViewController () {
  MDCAppBar* _appBar;
  NSArray* _sections;
  NSMutableArray* _content;
}
@end

@implementation RemotingSettingsViewController

@synthesize delegate = _delegate;
@synthesize inputMode = _inputMode;
@synthesize shouldResizeHostToFit = _shouldResizeHostToFit;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _appBar = [[MDCAppBar alloc] init];
  [self addChildViewController:_appBar.headerViewController];

  self.view.backgroundColor = RemotingTheme.menuBlueColor;
  _appBar.headerViewController.headerView.backgroundColor =
      RemotingTheme.menuBlueColor;
  MDCNavigationBarTextColorAccessibilityMutator* mutator =
      [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
  [mutator mutate:_appBar.navigationBar];

  _appBar.headerViewController.headerView.trackingScrollView =
      self.collectionView;
  [_appBar addSubviewsToParent];

  self.collectionView.backgroundColor = RemotingTheme.menuBlueColor;

  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithImage:RemotingTheme.closeIcon
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapClose:)];
  remoting::SetAccessibilityInfoFromImage(closeButton);
  self.navigationItem.leftBarButtonItem = nil;
  self.navigationItem.rightBarButtonItem = closeButton;

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forCellWithReuseIdentifier:kReusableIdentifierItem];

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:UICollectionElementKindSectionHeader];

  // TODO(nicholss): All of these strings need to be setup for l18n.
  _sections = @[
    l10n_util::GetNSString(IDS_DISPLAY_OPTIONS),
    l10n_util::GetNSString(IDS_MOUSE_OPTIONS),
    l10n_util::GetNSString(IDS_KEYBOARD_OPTIONS),
    l10n_util::GetNSString(IDS_SUPPORT_MENU),
  ];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];
  [self loadContent];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
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
  cell.textLabel.textColor = RemotingTheme.menuTextColor;
  cell.textLabel.numberOfLines = 1;
  cell.detailTextLabel.text = setting.subtext;
  cell.detailTextLabel.textColor = RemotingTheme.menuTextColor;
  cell.detailTextLabel.numberOfLines = 1;
  cell.tintColor = RemotingTheme.menuBlueColor;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel =
      [NSString stringWithFormat:@"%@\n%@", setting.title, setting.subtext];

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

  SettingOption* setting = _content[indexPath.section][indexPath.item];

  NSMutableArray* updatedIndexPaths = [NSMutableArray arrayWithCapacity:1];
  int i = 0;
  switch (setting.style) {
    case OptionCheckbox:
      setting.checked = !setting.checked;
      [updatedIndexPaths
          addObject:[NSIndexPath indexPathForItem:indexPath.item
                                        inSection:indexPath.section]];
      break;
    case OptionSelector:
      for (SettingOption* s in _content[indexPath.section]) {
        s.checked = NO;
        [updatedIndexPaths
            addObject:[NSIndexPath indexPathForItem:i
                                          inSection:indexPath.section]];
        i++;
      }
      setting.checked = YES;
      break;
    case FlatButton:
      break;
  }
  [self.collectionView reloadItemsAtIndexPaths:updatedIndexPaths];
  if (setting.action) {
    setting.action();
  }
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
    supplementaryView.textLabel.textColor = RemotingTheme.menuTextColor;
    supplementaryView.isAccessibilityElement = YES;
    supplementaryView.accessibilityLabel = supplementaryView.textLabel.text;
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

- (void)loadContent {
  _content = [NSMutableArray array];

  __weak RemotingSettingsViewController* weakSelf = self;

// We are not going to support the shrink option for now.
#if 0
  SettingOption* shrinkOption = [[SettingOption alloc] init];
  shrinkOption.title = l10n_util::GetNSString(IDS_SHRINK_TO_FIT);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  shrinkOption.subtext = l10n_util::GetNSString(IDS_SHRINK_TO_FIT_SUBTITLE);
  shrinkOption.style = OptionCheckbox;
  shrinkOption.checked = NO;
  __weak SettingOption* weakShrinkOption = shrinkOption;
  shrinkOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(setShrinkToFit:)]) {
      [weakSelf.delegate setShrinkToFit:weakShrinkOption.checked];
    }
  };
#endif

  SettingOption* resizeOption = [[SettingOption alloc] init];
  resizeOption.title = l10n_util::GetNSString(IDS_RESIZE_TO_CLIENT);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  resizeOption.subtext = l10n_util::GetNSString(IDS_RESIZE_TO_CLIENT_SUBTITLE);
  resizeOption.style = OptionCheckbox;
  resizeOption.checked = self.shouldResizeHostToFit;
  __weak SettingOption* weakResizeOption = resizeOption;
  resizeOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(setResizeToFit:)]) {
      [weakSelf.delegate setResizeToFit:weakResizeOption.checked];
    }
  };

  [_content addObject:@[ resizeOption ]];

  SettingOption* directMode = [[SettingOption alloc] init];
  directMode.title = l10n_util::GetNSString(IDS_SELECT_TOUCH_MODE);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  directMode.subtext = l10n_util::GetNSString(IDS_TOUCH_MODE_DESCRIPTION);
  directMode.style = OptionSelector;
  directMode.checked =
      self.inputMode == remoting::GestureInterpreter::DIRECT_INPUT_MODE;
  directMode.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(useDirectInputMode)]) {
      [weakSelf.delegate useDirectInputMode];
    }
  };

  SettingOption* trackpadMode = [[SettingOption alloc] init];
  trackpadMode.title = l10n_util::GetNSString(IDS_SELECT_TRACKPAD_MODE);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  trackpadMode.subtext = l10n_util::GetNSString(IDS_TRACKPAD_MODE_DESCRIPTION);
  trackpadMode.style = OptionSelector;
  trackpadMode.checked =
      self.inputMode == remoting::GestureInterpreter::TRACKPAD_INPUT_MODE;
  trackpadMode.action = ^{
    if ([weakSelf.delegate
            respondsToSelector:@selector(useTrackpadInputMode)]) {
      [weakSelf.delegate useTrackpadInputMode];
    }
  };

  [_content addObject:@[ directMode, trackpadMode ]];

  SettingOption* ctrlAltDelOption = [[SettingOption alloc] init];
  ctrlAltDelOption.title = l10n_util::GetNSString(IDS_SEND_CTRL_ALT_DEL);
  ctrlAltDelOption.style = FlatButton;
  ctrlAltDelOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendCtrAltDel)]) {
      [weakSelf.delegate sendCtrAltDel];
      [weakSelf dismissViewControllerAnimated:YES completion:nil];
    }
  };

  SettingOption* printScreenOption = [[SettingOption alloc] init];
  printScreenOption.title = l10n_util::GetNSString(IDS_SEND_PRINT_SCREEN);
  printScreenOption.style = FlatButton;
  printScreenOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendPrintScreen)]) {
      [weakSelf.delegate sendPrintScreen];
      [weakSelf dismissViewControllerAnimated:YES completion:nil];
    }
  };

  [_content addObject:@[ ctrlAltDelOption, printScreenOption ]];

  SettingOption* helpCenterOption = [[SettingOption alloc] init];
  helpCenterOption.title = l10n_util::GetNSString(IDS_HELP_CENTER);
  helpCenterOption.style = FlatButton;
  helpCenterOption.action = ^{
    [AppDelegate.instance navigateToHelpCenter:weakSelf.navigationController];
    [weakSelf.navigationController setNavigationBarHidden:NO animated:YES];
  };

  // TODO(yuweih): Currently the EAGLView is not captured by the feedback tool.
  // To get it working we need to override renderInContext in CAEAGLLayer.
  SettingOption* sendFeedbackOption = [[SettingOption alloc] init];
  sendFeedbackOption.title =
      l10n_util::GetNSString(IDS_ACTIONBAR_SEND_FEEDBACK);
  sendFeedbackOption.style = FlatButton;
  sendFeedbackOption.action = ^{
    // Dismiss self so that it can capture the screenshot of HostView.
    [weakSelf dismissViewControllerAnimated:YES
                                 completion:^{
                                   [AppDelegate.instance
                                       presentFeedbackFlowWithContext:
                                           kFeedbackContext];
                                 }];
  };

  [_content addObject:@[ helpCenterOption, sendFeedbackOption ]];

  DCHECK_EQ(_content.count, _sections.count);
}

@end
