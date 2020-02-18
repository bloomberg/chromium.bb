// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_session_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeSessionControllerDelegate

- (web::NavigationItemImpl*)pendingItemForSessionController:
    (CRWSessionController*)sessionController {
  return self.pendingItem;
}

@end
