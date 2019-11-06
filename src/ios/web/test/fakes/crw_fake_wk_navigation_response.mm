// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_wk_navigation_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWKNavigationResponse
@synthesize forMainFrame = _forMainFrame;
@synthesize response = _response;
@synthesize canShowMIMEType = _canShowMIMEType;
@end
