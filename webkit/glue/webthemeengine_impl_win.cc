// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_win.h"

#include <vsstyle.h>  // To convert to gfx::NativeTheme::State

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "ui/gfx/native_theme_win.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace webkit_glue {

static RECT WebRectToRECT(const WebRect& rect) {
  RECT result;
  result.left = rect.x;
  result.top = rect.y;
  result.right = rect.x + rect.width;
  result.bottom = rect.y + rect.height;
  return result;
}

static gfx::NativeTheme::State WebButtonStateToGfx(
    int part,
    int state,
    gfx::NativeTheme::ButtonExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;

  if (part == BP_PUSHBUTTON) {
    switch(state) {
      case PBS_NORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_HOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_PRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_DISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_DEFAULTED:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      case PBS_DEFAULTED_ANIMATING:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
    }
  } else if (part == BP_RADIOBUTTON) {
    switch(state) {
      case RBS_UNCHECKEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
        break;
    }
  } else if (part == BP_CHECKBOX) {
    switch(state) {
      case CBS_UNCHECKEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_MIXEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_IMPLICITNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDNORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDHOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDPRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDDISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
        break;
    }
  } else if (part == BP_GROUPBOX) {
    switch(state) {
      case GBS_NORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case GBS_DISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
        break;
    }
  } else if (part == BP_COMMANDLINK) {
    switch(state) {
      case CMDLS_NORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_HOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_PRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_DISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_DEFAULTED:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      case CMDLS_DEFAULTED_ANIMATING:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
        break;
    }
  } else if (part == BP_COMMANDLINKGLYPH) {
    switch(state) {
      case CMDLGS_NORMAL:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_HOT:
        gfx_state = gfx::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_PRESSED:
        gfx_state = gfx::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_DISABLED:
        gfx_state = gfx::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_DEFAULTED:
        gfx_state = gfx::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      default:
        NOTREACHED() << "Invalid state: " << state;
        break;
    }
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  gfx::NativeTheme::Part native_part = gfx::NativeTheme::kPushButton;
  switch(part) {
    case BP_PUSHBUTTON:
      native_part = gfx::NativeTheme::kPushButton;
      break;
    case BP_CHECKBOX:
      native_part = gfx::NativeTheme::kCheckbox;
      break;
    case BP_RADIOBUTTON:
      native_part = gfx::NativeTheme::kRadio;
      break;
    default:
      break;
  }
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebButtonStateToGfx(part, state,
                                                             &extra.button);
  extra.button.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

void WebThemeEngineImpl::paintMenuList(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintMenuList(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarArrow(
    WebCanvas* canvas, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarArrow(
      hdc, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarThumb(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarThumb(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintScrollbarTrack(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, const WebRect& align_rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  RECT native_align_rect = WebRectToRECT(align_rect);
  gfx::NativeThemeWin::instance()->PaintScrollbarTrack(
      hdc, part, state, classic_state, &native_rect, &native_align_rect,
      canvas);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintSpinButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintSpinButton(
      hdc, part, state, classic_state, &native_rect);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintTextField(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, WebColor color, bool fill_content_area,
    bool draw_edges) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  COLORREF c = skia::SkColorToCOLORREF(color);

  gfx::NativeThemeWin::instance()->PaintTextField(
      hdc, part, state, classic_state, &native_rect, c, fill_content_area,
      draw_edges);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintTrackbar(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_rect = WebRectToRECT(rect);
  gfx::NativeThemeWin::instance()->PaintTrackbar(
      hdc, part, state, classic_state, &native_rect, canvas);

  skia::EndPlatformPaint(canvas);
}

void WebThemeEngineImpl::paintProgressBar(
    WebCanvas* canvas, const WebRect& barRect, const WebRect& valueRect,
    bool determinate, double animatedSeconds)
{
  HDC hdc = skia::BeginPlatformPaint(canvas);

  RECT native_bar_rect = WebRectToRECT(barRect);
  RECT native_value_rect = WebRectToRECT(valueRect);
  gfx::NativeThemeWin::instance()->PaintProgressBar(
      hdc, &native_bar_rect,
      &native_value_rect, determinate, animatedSeconds, canvas);

  skia::EndPlatformPaint(canvas);
}

}  // namespace webkit_glue
