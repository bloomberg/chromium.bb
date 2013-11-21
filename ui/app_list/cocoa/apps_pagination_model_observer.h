// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_APPS_PAGINATION_MODEL_OBSERVER_H_
#define UI_APP_LIST_COCOA_APPS_PAGINATION_MODEL_OBSERVER_H_

// Observer protocol for page changes. Compare with
// app_list::PaginationModelObserver.
@protocol AppsPaginationModelObserver

// Invoked when the total number of pages has changed.
- (void)totalPagesChanged;

// Invoked when the selected page index is changed.
- (void)selectedPageChanged:(int)newSelected;

// Invoked when the portion of pages that are visible have changed.
- (void)pageVisibilityChanged;

// Return a pager segment at |locationInWindow| or -1 if there is none.
- (NSInteger)pagerSegmentAtLocation:(NSPoint)locationInWindow;

@end

#endif  // UI_APP_LIST_COCOA_APPS_PAGINATION_MODEL_OBSERVER_H_
