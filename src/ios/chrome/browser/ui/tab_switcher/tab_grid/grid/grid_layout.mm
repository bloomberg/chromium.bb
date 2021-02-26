// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/reordering_layout_util.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GridLayout

#pragma mark - UICollectionViewLayout

// This is called whenever the layout is invalidated, including during rotation.
// Resizes item, margins, and spacing to fit new size classes and width.
- (void)prepareLayout {
  [super prepareLayout];

  UIUserInterfaceSizeClass horizontalSizeClass =
      self.collectionView.traitCollection.horizontalSizeClass;
  UIUserInterfaceSizeClass verticalSizeClass =
      self.collectionView.traitCollection.verticalSizeClass;
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    self.itemSize = kGridCellSizeAccessibility;
    self.sectionInset = kGridLayoutInsetsRegularCompact;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularCompact;
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    if (width < kGridLayoutCompactCompactLimitedWidth) {
      self.sectionInset = kGridLayoutInsetsCompactCompactLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactCompactLimitedWidth;
    } else {
      self.sectionInset = kGridLayoutInsetsCompactCompact;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactCompact;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
             verticalSizeClass == UIUserInterfaceSizeClassRegular) {
    if (width < kGridLayoutCompactRegularLimitedWidth) {
      self.itemSize = kGridCellSizeSmall;
      self.sectionInset = kGridLayoutInsetsCompactRegularLimitedWidth;
      self.minimumLineSpacing =
          kGridLayoutLineSpacingCompactRegularLimitedWidth;
    } else {
      self.itemSize = kGridCellSizeMedium;
      self.sectionInset = kGridLayoutInsetsCompactRegular;
      self.minimumLineSpacing = kGridLayoutLineSpacingCompactRegular;
    }
  } else if (horizontalSizeClass == UIUserInterfaceSizeClassRegular &&
             verticalSizeClass == UIUserInterfaceSizeClassCompact) {
    self.itemSize = kGridCellSizeSmall;
    self.sectionInset = kGridLayoutInsetsRegularCompact;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularCompact;
  } else {
    self.itemSize = kGridCellSizeLarge;
    self.sectionInset = kGridLayoutInsetsRegularRegular;
    self.minimumLineSpacing = kGridLayoutLineSpacingRegularRegular;
  }
  if (IsThumbStripEnabled()) {
    // When the thumb strip feature is enabled, increase the bottom inset to
    // account for the bvc on the bottom of the screen.
    UIEdgeInsets sectionInset = self.sectionInset;
    sectionInset.bottom += kBVCHeightTabGrid;
    self.sectionInset = sectionInset;
  }
}

@end

@implementation GridReorderingLayout

#pragma mark - UICollectionViewLayout

// Both -layoutAttributesForElementsInRect: and
// -layoutAttributesForItemAtIndexPath: need to be overridden to change the
// default layout attributes.
- (NSArray<__kindof UICollectionViewLayoutAttributes*>*)
    layoutAttributesForElementsInRect:(CGRect)rect {
  return CopyAttributesArrayAndSetInactiveOpacity(
      [super layoutAttributesForElementsInRect:rect]);
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  return CopyAttributesAndSetInactiveOpacity(
      [super layoutAttributesForItemAtIndexPath:indexPath]);
}

- (UICollectionViewLayoutAttributes*)
    layoutAttributesForInteractivelyMovingItemAtIndexPath:
        (NSIndexPath*)indexPath
                                       withTargetPosition:(CGPoint)position {
  return CopyAttributesAndSetActiveProperties([super
      layoutAttributesForInteractivelyMovingItemAtIndexPath:indexPath
                                         withTargetPosition:position]);
}

@end
