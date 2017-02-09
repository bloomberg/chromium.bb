// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_HOST_COLLECTION_VIEW_CONTOLLER_H_
#define REMOTING_CLIENT_IOS_APP_HOST_COLLECTION_VIEW_CONTOLLER_H_

#import <UIKit/UIKit.h>

#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"
#import "ios/third_party/material_components_ios/src/components/FlexibleHeader/src/MaterialFlexibleHeader.h"
#import "remoting/client/ios/app/host_collection_view_cell.h"
#include "remoting/client/ios/domain/host.h"

@protocol HostCollectionViewControllerDelegate<NSObject>

@optional

- (void)didSelectCell:(HostCollectionViewCell*)cell
           completion:(void (^)())completionBlock;

- (Host*)getHostAtIndexPath:(NSIndexPath*)path;
- (NSInteger)getHostCount;

@end

@interface HostCollectionViewController : MDCCollectionViewController

@property(weak, nonatomic) id<HostCollectionViewControllerDelegate> delegate;
@property(nonatomic) CGFloat scrollOffsetY;
@property(nonatomic)
    MDCFlexibleHeaderContainerViewController* flexHeaderContainerVC;

@end

#endif  // REMOTING_CLIENT_IOS_APP_HOST_LIST_VIEW_CONTOLLER_H_
