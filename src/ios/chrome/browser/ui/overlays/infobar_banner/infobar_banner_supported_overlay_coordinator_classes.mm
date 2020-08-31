// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace infobar_banner {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  return @ [[InfobarBannerOverlayCoordinator class]];
}

}  // infobar_banner
