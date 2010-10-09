// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_linux.h"

#include "gfx/native_theme_linux.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace webkit_glue {

static gfx::Rect WebRectToRect(const WebRect& rect) {
  return gfx::Rect(rect.x, rect.y, rect.width, rect.height);
}

static gfx::NativeThemeLinux::Part NativeThemePart(
    WebKit::WebThemeEngine::Part part) {
  switch (part) {
    case WebKit::WebThemeEngine::PartScrollbarDownArrow:
      return gfx::NativeThemeLinux::kScrollbarDownArrow;
    case WebKit::WebThemeEngine::PartScrollbarLeftArrow:
      return gfx::NativeThemeLinux::kScrollbarLeftArrow;
    case WebKit::WebThemeEngine::PartScrollbarRightArrow:
      return gfx::NativeThemeLinux::kScrollbarRightArrow;
    case WebKit::WebThemeEngine::PartScrollbarUpArrow:
      return gfx::NativeThemeLinux::kScrollbarUpArrow;
    case WebKit::WebThemeEngine::PartScrollbarHorizontalThumb:
      return gfx::NativeThemeLinux::kScrollbarHorizontalThumb;
    case WebKit::WebThemeEngine::PartScrollbarVerticalThumb:
      return gfx::NativeThemeLinux::kScrollbarVerticalThumb;
    case WebKit::WebThemeEngine::PartScrollbarHoriztonalTrack:
      return gfx::NativeThemeLinux::kScrollbarHorizontalTrack;
    case WebKit::WebThemeEngine::PartScrollbarVerticalTrack:
      return gfx::NativeThemeLinux::kScrollbarVerticalTrack;
    default:
      return gfx::NativeThemeLinux::kScrollbarDownArrow;
  }
}

static gfx::NativeThemeLinux::State NativeThemeState(
    WebKit::WebThemeEngine::State state) {
  switch (state) {
    case WebKit::WebThemeEngine::StateDisabled:
      return gfx::NativeThemeLinux::kDisabled;
    case WebKit::WebThemeEngine::StateHover:
      return gfx::NativeThemeLinux::kHover;
    case WebKit::WebThemeEngine::StateNormal:
      return gfx::NativeThemeLinux::kNormal;
    case WebKit::WebThemeEngine::StatePressed:
      return gfx::NativeThemeLinux::kPressed;
    default:
      return gfx::NativeThemeLinux::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebKit::WebThemeEngine::Part part,
    WebKit::WebThemeEngine::State state,
    const WebKit::WebThemeEngine::ExtraParams* extra_params,
    gfx::NativeThemeLinux::ExtraParams* native_theme_extra_params) {
  if (part == WebKit::WebThemeEngine::PartScrollbarHoriztonalTrack ||
      part == WebKit::WebThemeEngine::PartScrollbarVerticalTrack) {
    native_theme_extra_params->scrollbar_track.track_x =
        extra_params->scrollbarTrack.trackX;
    native_theme_extra_params->scrollbar_track.track_y =
        extra_params->scrollbarTrack.trackY;
    native_theme_extra_params->scrollbar_track.track_width =
        extra_params->scrollbarTrack.trackWidth;
    native_theme_extra_params->scrollbar_track.track_height =
        extra_params->scrollbarTrack.trackHeight;
  }
}

WebKit::WebSize WebThemeEngineImpl::getSize(WebKit::WebThemeEngine::Part part) {
  return gfx::NativeThemeLinux::instance()->GetSize(NativeThemePart(part));
}

void WebThemeEngineImpl::paint(
    WebKit::WebCanvas* canvas,
    WebKit::WebThemeEngine::Part part,
    WebKit::WebThemeEngine::State state,
    const WebKit::WebRect& rect,
    const WebKit::WebThemeEngine::ExtraParams* extra_params) {
  gfx::NativeThemeLinux::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  gfx::NativeThemeLinux::instance()->Paint(
      canvas,
      NativeThemePart(part),
      NativeThemeState(state),
      WebRectToRect(rect),
      native_theme_extra_params);
}
}  // namespace webkit_glue
