// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/web/public/features.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void MoveContentBelowHeader(id<CRWWebViewProxy> proxy, FullscreenModel* model) {
  DCHECK(proxy);
  DCHECK(model);
  CGFloat topPadding = model->current_toolbar_insets().top;
  proxy.scrollViewProxy.contentOffset = CGPointMake(0, -topPadding);
  if (!base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen)) {
    // With the fullscreen implementation living outside of web, this is no
    // longer needed.
    CGFloat bottomPadding = model->progress() * model->GetBottomToolbarHeight();
    UIEdgeInsets contentInset = proxy.contentInset;
    contentInset.top = topPadding;
    contentInset.bottom = bottomPadding;
    proxy.contentInset = contentInset;
  }
}
