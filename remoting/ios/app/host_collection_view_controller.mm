// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_collection_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"

static NSString* const kReusableIdentifierItem =
    @"remotingHostCollectionViewControllerItem";

static CGFloat kHostCollectionViewControllerCellHeight = 70.f;
static CGFloat kHostCollectionViewControllerDefaultHeaderHeight = 100.f;
static CGFloat kHostCollectionViewControllerSmallHeaderHeight = 60.f;
static UIColor* kBackgroundColor =
    [UIColor colorWithRed:0.f green:0.67f blue:0.55f alpha:1.f];

@interface HostCollectionViewController () {
  MDCInkTouchController* _inkTouchController;
}
@end

@implementation HostCollectionViewController

@synthesize delegate = _delegate;
@synthesize flexHeaderContainerViewController =
    _flexHeaderContainerViewController;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout {
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    self.collectionView.backgroundColor = [UIColor whiteColor];
    [self.collectionView registerClass:[HostCollectionViewCell class]
            forCellWithReuseIdentifier:NSStringFromClass(
                                           [HostCollectionViewCell class])];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.cellLayoutType = MDCCollectionViewCellLayoutTypeGrid;
  self.styler.gridPadding = 0;
  self.styler.gridColumnCount = 1;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [_delegate getHostCount];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  HostCollectionViewCell* cell =
      [collectionView dequeueReusableCellWithReuseIdentifier:
                          NSStringFromClass([HostCollectionViewCell class])
                                                forIndexPath:indexPath];
  HostInfo* host = [_delegate getHostAtIndexPath:indexPath];
  if (host) {
    [cell populateContentWithHostInfo:host];
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  HostCollectionViewCell* cell = (HostCollectionViewCell*)[collectionView
      cellForItemAtIndexPath:indexPath];
  [_delegate didSelectCell:cell
                completion:^{
                }];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  return kHostCollectionViewControllerCellHeight;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.flexHeaderContainerViewController.headerViewController
      scrollViewDidScroll:scrollView];
}

#pragma mark - Private

- (UIView*)hostsHeaderView {
  CGRect headerFrame =
      _flexHeaderContainerViewController.headerViewController.headerView.bounds;
  UIView* hostsHeaderView = [[UIView alloc] initWithFrame:headerFrame];
  hostsHeaderView.backgroundColor = kBackgroundColor;
  hostsHeaderView.layer.masksToBounds = YES;
  hostsHeaderView.autoresizingMask =
      (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);

  _inkTouchController =
      [[MDCInkTouchController alloc] initWithView:hostsHeaderView];
  [_inkTouchController addInkView];

  return hostsHeaderView;
}

- (void)setflexHeaderContainerViewController:
    (MDCFlexibleHeaderContainerViewController*)
        flexHeaderContainerViewController {
  _flexHeaderContainerViewController = flexHeaderContainerViewController;
  MDCFlexibleHeaderView* headerView =
      _flexHeaderContainerViewController.headerViewController.headerView;
  headerView.trackingScrollView = self.collectionView;
  headerView.maximumHeight = kHostCollectionViewControllerDefaultHeaderHeight;
  headerView.minimumHeight = kHostCollectionViewControllerSmallHeaderHeight;
  [headerView addSubview:[self hostsHeaderView]];

  // Use a custom shadow under the flexible header.
  MDCShadowLayer* shadowLayer = [MDCShadowLayer layer];
  [headerView setShadowLayer:shadowLayer
      intensityDidChangeBlock:^(CALayer* layer, CGFloat intensity) {
        CGFloat elevation = MDCShadowElevationAppBar * intensity;
        [(MDCShadowLayer*)layer setElevation:elevation];
      }];
}

@end
