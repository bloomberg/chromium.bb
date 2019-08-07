// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_web_state_list_delegate.h"

#import "ios/chrome/browser/tabs/tab_helper_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabModelWebStateListDelegate::TabModelWebStateListDelegate() = default;

TabModelWebStateListDelegate::~TabModelWebStateListDelegate() = default;

void TabModelWebStateListDelegate::WillAddWebState(web::WebState* web_state) {
  // Unconditionally call AttachTabHelper even for pre-rendered WebState as
  // the method is idempotent and this ensure that any WebState in a TabModel
  // has all the expected tab helpers.
  AttachTabHelpers(web_state, /*for_prerender=*/false);
}

void TabModelWebStateListDelegate::WebStateDetached(web::WebState* web_state) {}
