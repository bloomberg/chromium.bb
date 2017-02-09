// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_HOST_COLLECTION_VIEW_CELL_H_
#define REMOTING_CLIENT_IOS_APP_HOST_COLLECTION_VIEW_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"

@interface HostCollectionViewCell : MDCCollectionViewCell

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* status;

- (void)populateContentWithTitle:(NSString*)title status:(NSString*)status;

@end

#endif  // REMOTING_CLIENT_IOS_APP_HOST_COLLECTION_VIEW_CELL_H_
