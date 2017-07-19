// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_setup_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#import "remoting/ios/app/host_setup_footer_view.h"
#import "remoting/ios/app/host_setup_header_view.h"
#import "remoting/ios/app/host_setup_view_cell.h"
#import "remoting/ios/app/remoting_theme.h"

static NSString* const kReusableIdentifierItem = @"remotingSetupStepVCItem";

// TODO(yuweih): i18n
// Clang doesn't allow string literal concatenation in string array but
// `git cl format` will break down long lines into multiple string literals.
// This is to disable the warning until we move these to a better place.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-string-concatenation"
static NSArray<NSString*>* const kSetupSteps = @[
  @"Go to the computer that you want to remotely access",
  @"Visit google.com/remotedesktop from your Chrome browser",
  @"Install the Chrome Remote Desktop software and follow the instructions to "
   "complete the setup",
  @"After your remote device is setup, you can access it directly from this "
   "page",
];
#pragma clang diagnostic pop

static const CGFloat kHeaderHeight = 80.f;
static const CGFloat kFooterHeight = 15.f;  // 80.0f for HostSetupFooterView

@implementation HostSetupViewController

@synthesize scrollViewDelegate = _scrollViewDelegate;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout {
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    self.collectionView.backgroundColor = UIColor.clearColor;
    [self.collectionView registerClass:[HostSetupViewCell class]
            forCellWithReuseIdentifier:kReusableIdentifierItem];

    [self.collectionView registerClass:[HostSetupHeaderView class]
            forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                   withReuseIdentifier:UICollectionElementKindSectionHeader];

    // TODO(yuweih): Use HostSetupFooterView once we have the email instructions
    // feature.
    [self.collectionView registerClass:[UICollectionReusableView class]
            forSupplementaryViewOfKind:UICollectionElementKindSectionFooter
                   withReuseIdentifier:UICollectionElementKindSectionFooter];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.cellStyle = MDCCollectionViewCellStyleGrouped;
  self.styler.cellLayoutType = MDCCollectionViewCellLayoutTypeList;
  self.styler.shouldHideSeparators = YES;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return kSetupSteps.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  HostSetupViewCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  [cell setContentText:kSetupSteps[indexPath.item] number:indexPath.item + 1];
  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  UICollectionReusableView* view =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if (kind == UICollectionElementKindSectionFooter) {
    // TODO(yuweih): No longer necessary once we use HostSetupFooterView.
    view.backgroundColor = RemotingTheme.setupListBackgroundColor;
  }
  return view;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  return MDCCellDefaultThreeLineHeight;
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width, kHeaderHeight);
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForFooterInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width, kFooterHeight);
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [_scrollViewDelegate scrollViewDidScroll:scrollView];
}

@end
