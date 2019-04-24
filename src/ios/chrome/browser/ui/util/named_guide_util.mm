// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/named_guide_util.h"

#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AddNamedGuidesToView(NSArray<GuideName*>* names, UIView* view) {
  for (GuideName* name in names) {
    [view addLayoutGuide:[[NamedGuide alloc] initWithName:name]];
  }
}

void SetNamedGuideConstrainedViews(
    NSDictionary<GuideName*, UIView*>* views_for_names) {
  for (GuideName* name in [views_for_names allKeys]) {
    UIView* view = views_for_names[name];
    [NamedGuide guideWithName:name view:view].constrainedView = view;
  }
}
