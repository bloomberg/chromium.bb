// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_
#define UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {
class SearchBoxModel;
class SearchBoxModelObserverBridge;
}

@class SearchTextField;

@protocol AppsSearchBoxDelegate<NSTextFieldDelegate>

- (app_list::SearchBoxModel*)searchBoxModel;
- (void)modelTextDidChange;

@end

// Controller for the search box in the topmost portion of the app list.
APP_LIST_EXPORT
@interface AppsSearchBoxController : NSViewController<NSTextFieldDelegate> {
 @private
  scoped_nsobject<SearchTextField> searchTextField_;
  scoped_nsobject<NSImageView> searchImageView_;
  scoped_ptr<app_list::SearchBoxModelObserverBridge> bridge_;

  id<AppsSearchBoxDelegate> delegate_;  // Weak. Owns us.
}

@property(assign, nonatomic) id<AppsSearchBoxDelegate> delegate;

- (id)initWithFrame:(NSRect)frame;
- (void)clearSearch;

@end

@interface AppsSearchBoxController (TestingAPI)

- (NSTextField*)searchTextField;

@end

#endif  // UI_APP_LIST_COCOA_APPS_SEARCH_BOX_CONTROLLER_H_
