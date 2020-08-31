// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator.h"

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeOverlayRequestCoordinator

+ (const OverlayRequestSupport*)requestSupport {
  return OverlayRequestSupport::All();
}

- (void)startAnimated:(BOOL)animated {
}

- (void)stopAnimated:(BOOL)animated {
}

@end
