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

#include "base/logging.h"

static NSString* const kReusableIdentifierItem = @"remotingSettingsVCItem";

@interface RemotingSettingsViewController () {
  MDCAppBar* _appBar;
  NSArray* _sections;
  NSMutableArray* _content;
}
@end

@implementation RemotingSettingsViewController

@synthesize delegate = _delegate;
@synthesize inputMode = _inputMode;

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
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];
  [self loadContent];
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

- (void)loadContent {
  _content = [NSMutableArray array];

  __weak RemotingSettingsViewController* weakSelf = self;

  SettingOption* shrinkOption = [[SettingOption alloc] init];
  shrinkOption.title = @"Shrink to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  shrinkOption.subtext = @"Don't change resolution to match window";
  shrinkOption.style = OptionCheckbox;
  shrinkOption.checked = NO;
  __weak SettingOption* weakShrinkOption = shrinkOption;
  shrinkOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(setShrinkToFit:)]) {
      [weakSelf.delegate setShrinkToFit:weakShrinkOption.checked];
    }
  };

  SettingOption* resizeOption = [[SettingOption alloc] init];
  resizeOption.title = @"Resize to fit";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  resizeOption.subtext = @"Update remote resolution to match window";
  resizeOption.style = OptionCheckbox;
  resizeOption.checked = YES;
  __weak SettingOption* weakResizeOption = resizeOption;
  resizeOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(setResizeToFit:)]) {
      [weakSelf.delegate setResizeToFit:weakResizeOption.checked];
    }
  };

  [_content addObject:@[ shrinkOption, resizeOption ]];

  SettingOption* directMode = [[SettingOption alloc] init];
  directMode.title = @"Touch mode";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  directMode.subtext = @"Screen acts like a touch screen";
  directMode.style = OptionSelector;
  directMode.checked =
      self.inputMode == remoting::GestureInterpreter::DIRECT_INPUT_MODE;
  directMode.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(useDirectInputMode)]) {
      [weakSelf.delegate useDirectInputMode];
    }
  };

  SettingOption* trackpadMode = [[SettingOption alloc] init];
  trackpadMode.title = @"Trackpad mode";
  // TODO(nicholss): I think this text changes based on value. Confirm.
  trackpadMode.subtext = @"Screen acts like a trackpad";
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
  ctrlAltDelOption.title = @"Press \"Ctrl+Alt+Del\"";
  ctrlAltDelOption.style = FlatButton;
  ctrlAltDelOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendCtrAltDel)]) {
      [weakSelf.delegate sendCtrAltDel];
    }
  };

  SettingOption* printScreenOption = [[SettingOption alloc] init];
  printScreenOption.title = @"Press \"Print Screen\"";
  printScreenOption.style = FlatButton;
  printScreenOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendPrintScreen)]) {
      [weakSelf.delegate sendPrintScreen];
    }
  };

  [_content addObject:@[ ctrlAltDelOption, printScreenOption ]];

  SettingOption* helpCenterOption = [[SettingOption alloc] init];
  helpCenterOption.title = @"Help center";
  helpCenterOption.style = FlatButton;
  helpCenterOption.action = ^{
    [AppDelegate.instance navigateToHelpCenter:weakSelf.navigationController];
    [weakSelf.navigationController setNavigationBarHidden:NO animated:YES];
  };

  SettingOption* faqsOption = [[SettingOption alloc] init];
  faqsOption.title = @"FAQs";
  faqsOption.style = FlatButton;
  faqsOption.action = ^{
    [AppDelegate.instance navigateToFAQs:weakSelf.navigationController];
    [weakSelf.navigationController setNavigationBarHidden:NO animated:YES];
  };

  SettingOption* sendFeedbackOption = [[SettingOption alloc] init];
  sendFeedbackOption.title = @"Send feedback";
  sendFeedbackOption.style = FlatButton;
  sendFeedbackOption.action = ^{
    [AppDelegate.instance navigateToSendFeedback:self.navigationController];
    [weakSelf.navigationController setNavigationBarHidden:NO animated:YES];
  };

  [_content addObject:@[ helpCenterOption, faqsOption, sendFeedbackOption ]];

  DCHECK_EQ(_content.count, _sections.count);
}

@end
