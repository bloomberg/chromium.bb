// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_mac.h"

#include <Carbon/Carbon.h>

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"

#if WEBKIT_USING_SKIA
#include "skia/ext/skia_utils_mac.h"
#endif

using WebKit::WebCanvas;
using WebKit::WebRect;
using WebKit::WebThemeEngine;

namespace webkit_glue {

static ThemeTrackEnableState stateToHIEnableState(WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::StateDisabled:
      return kThemeTrackDisabled;
    case WebThemeEngine::StateInactive:
      return kThemeTrackInactive;
    default:
      return kThemeTrackActive;
  }
}

void WebThemeEngineImpl::paintScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo) {
  HIThemeTrackDrawInfo trackInfo;
  trackInfo.version = 0;
  trackInfo.kind = size == WebThemeEngine::SizeRegular ?
      kThemeMediumScrollBar : kThemeSmallScrollBar;
  trackInfo.bounds = CGRectMake(rect.x, rect.y, rect.width, rect.height);
  trackInfo.min = 0;
  trackInfo.max = scrollbarInfo.maxValue;
  trackInfo.value = scrollbarInfo.currentValue;
  trackInfo.trackInfo.scrollbar.viewsize = scrollbarInfo.visibleSize;
  trackInfo.attributes = 0;
  if (scrollbarInfo.orientation ==
      WebThemeEngine::ScrollbarOrientationHorizontal) {
    trackInfo.attributes |= kThemeTrackHorizontal;
  }

  trackInfo.enableState = stateToHIEnableState(state);

  trackInfo.trackInfo.scrollbar.pressState =
      state == WebThemeEngine::StatePressed ? kThemeThumbPressed : 0;
  trackInfo.attributes |= (kThemeTrackShowThumb | kThemeTrackHideTrack);
#if WEBKIT_USING_SKIA
  gfx::SkiaBitLocker bitLocker(canvas);
  CGContextRef cgContext = bitLocker.cgContext();
#else
  CGContextRef cgContext = canvas;
#endif
  HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);
}

}  // namespace webkit_glue
