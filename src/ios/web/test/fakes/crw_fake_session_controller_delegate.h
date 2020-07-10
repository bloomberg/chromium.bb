// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_SESSION_CONTROLLER_DELEGATE_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_SESSION_CONTROLLER_DELEGATE_H_

#import "ios/web/navigation/crw_session_controller.h"

// Fake implementation of CRWSessionControllerDelegate, which allows to stub
// delegate method call results in tests.
@interface CRWFakeSessionControllerDelegate
    : NSObject <CRWSessionControllerDelegate>
// Pending item to be returned from pendingItemForSessionController: method.
@property(nonatomic) web::NavigationItemImpl* pendingItem;
@end

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_SESSION_CONTROLLER_DELEGATE_H_
