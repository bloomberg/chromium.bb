// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_win.h"

#include <vsstyle.h>  // To convert to gfx::NativeTheme::State

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "ui/gfx/native_theme.h"

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
    int part, int state, gfx::NativeTheme::ButtonExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;

  if (part == BP_PUSHBUTTON) {
    switch (state) {
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
    switch (state) {
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
    switch (state) {
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
    switch (state) {
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
    switch (state) {
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
    switch (state) {
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
  switch (part) {
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
      NOTREACHED() << "Invalid part: " << part;
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

static gfx::NativeTheme::State WebListMenuStateToGfx(int part, int state) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;

  switch (part) {
    case CP_DROPDOWNBUTTON:
      switch (state) {
        case CBXS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          break;
        case CBXS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          break;
        case CBXS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          break;
        case CBXS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintMenuList(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  gfx::NativeTheme::Part native_part = gfx::NativeTheme::kMenuList;
  switch (part) {
    case CP_DROPDOWNBUTTON:
      native_part = gfx::NativeTheme::kMenuList;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  gfx::NativeTheme::State native_state = WebListMenuStateToGfx(part, state);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::ExtraParams extra;
  extra.menu_list.classic_state = classic_state;
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static gfx::NativeTheme::State WebScrollbarArrowStateToGfx(
    int state, gfx::NativeTheme::Part* part,
    gfx::NativeTheme::ScrollbarArrowExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (state) {
    case ABS_UPNORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      *part = gfx::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPHOT:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPPRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      *part = gfx::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPDISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      *part = gfx::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNNORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      *part = gfx::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNHOT:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNPRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      *part = gfx::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNDISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      *part = gfx::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTNORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      *part = gfx::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTHOT:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTPRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      *part = gfx::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTDISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      *part = gfx::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTNORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      *part = gfx::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTHOT:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTPRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      *part = gfx::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTDISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      *part = gfx::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPHOVER:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = true;
      break;
    case ABS_DOWNHOVER:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = true;
      break;
    case ABS_LEFTHOVER:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = true;
      break;
    case ABS_RIGHTHOVER:
      gfx_state = gfx::NativeTheme::kHovered;
      *part = gfx::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = true;
      break;
    default:
      NOTREACHED() << "Invalid state: " << state;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintScrollbarArrow(
    WebCanvas* canvas, int state, int classic_state, const WebRect& rect) {
  gfx::NativeTheme::Part native_part;
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebScrollbarArrowStateToGfx(
      state, &native_part, &extra.scrollbar_arrow);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static gfx::NativeTheme::State WebScrollbarThumbStateToGfx(
    int state, gfx::NativeTheme::ScrollbarThumbExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (state) {
    case SCRBS_NORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      extra->is_hovering = false;
      break;
    case SCRBS_HOVER:
      gfx_state = gfx::NativeTheme::kHovered;
      extra->is_hovering = true;
      break;
    case SCRBS_HOT:
      gfx_state = gfx::NativeTheme::kHovered;
      extra->is_hovering = false;
      break;
    case SCRBS_PRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      extra->is_hovering = false;
      break;
    case SCRBS_DISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      extra->is_hovering = false;
      break;
    default:
      NOTREACHED() << "Invalid state: " << state;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintScrollbarThumb(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  gfx::NativeTheme::Part native_part;
  if (part == SBP_THUMBBTNHORZ) {
    native_part = gfx::NativeTheme::kScrollbarHorizontalThumb;
  } else if (part == SBP_THUMBBTNVERT) {
    native_part = gfx::NativeTheme::kScrollbarVerticalThumb;
  } else if (part == SBP_GRIPPERHORZ) {
    native_part = gfx::NativeTheme::kScrollbarHorizontalGripper;
  } else if (part == SBP_GRIPPERVERT) {
    native_part = gfx::NativeTheme::kScrollbarVerticalGripper;
  } else {
      NOTREACHED() << "Invalid part: " << part;
  }

  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebScrollbarThumbStateToGfx(
      state, &extra.scrollbar_thumb);

  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static gfx::NativeTheme::State WebScrollbarTrackStateToGfx(
    int part, int state, gfx::NativeTheme::Part* gfx_part,
    gfx::NativeTheme::ScrollbarTrackExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (part) {
    case SBP_LOWERTRACKHORZ:
      switch (state) {
        case SCRBS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    case SBP_UPPERTRACKHORZ:
      switch (state) {
        case SCRBS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          *gfx_part = gfx::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    case SBP_LOWERTRACKVERT:
      switch (state) {
        case SCRBS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    case SBP_UPPERTRACKVERT:
      switch (state) {
        case SCRBS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          *gfx_part = gfx::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintScrollbarTrack(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, const WebRect& align_rect) {
  gfx::NativeTheme::Part native_part;
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebScrollbarTrackStateToGfx(
      part, state, &native_part, &extra.scrollbar_track);
  extra.scrollbar_track.classic_state = classic_state;
  extra.scrollbar_track.track_x = align_rect.x;
  extra.scrollbar_track.track_y = align_rect.y;
  extra.scrollbar_track.track_width = align_rect.width;
  extra.scrollbar_track.track_height = align_rect.height;

  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static gfx::NativeTheme::State WebSpinButtonStateToGfx(
    int part, int state, gfx::NativeTheme::InnerSpinButtonExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (part) {
    case SPNP_UP:
      switch (state) {
        case UPS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    case SPNP_DOWN:
      switch (state) {
        case DNS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_PRESSED:
          gfx_state = gfx::NativeTheme::kPressed;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintSpinButton(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebSpinButtonStateToGfx(
      part, state, &extra.inner_spin);
  extra.inner_spin.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  gfx::NativeTheme::instance()->Paint(canvas,
                                      gfx::NativeTheme::kInnerSpinButton,
                                      native_state,
                                      gfx_rect,
                                      extra);
}

static gfx::NativeTheme::State WebTextFieldStateToGfx(
    int part, int state, gfx::NativeTheme::TextFieldExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (part) {
    case EP_EDITTEXT:
      switch (state) {
        case ETS_NORMAL:
          gfx_state = gfx::NativeTheme::kNormal;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_HOT:
          gfx_state = gfx::NativeTheme::kHovered;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_SELECTED:
          gfx_state = gfx::NativeTheme::kPressed;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_DISABLED:
          gfx_state = gfx::NativeTheme::kDisabled;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_FOCUSED:
          gfx_state = gfx::NativeTheme::kNormal;
          extra->is_read_only = false;
          extra->is_focused = true;
          break;
        case ETS_READONLY:
          gfx_state = gfx::NativeTheme::kNormal;
          extra->is_read_only = true;
          extra->is_focused = false;
          break;
        default:
          NOTREACHED() << "Invalid state: " << state;
          break;
      }
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  return gfx_state;
}

void WebThemeEngineImpl::paintTextField(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect, WebColor color, bool fill_content_area,
    bool draw_edges) {
  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebTextFieldStateToGfx(
      part, state, &extra.text_field);
  extra.text_field.fill_content_area = fill_content_area;
  extra.text_field.draw_edges = draw_edges;
  extra.text_field.background_color = color;
  extra.text_field.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);

  gfx::NativeTheme::instance()->Paint(canvas,
      gfx::NativeTheme::kTextField, native_state, gfx_rect, extra);
}

static gfx::NativeTheme::State WebTrackbarStateToGfx(
    int part,
    int state,
    gfx::NativeTheme::TrackbarExtraParams* extra) {
  gfx::NativeTheme::State gfx_state = gfx::NativeTheme::kNormal;
  switch (state) {
    case TUS_NORMAL:
      gfx_state = gfx::NativeTheme::kNormal;
      break;
    case TUS_HOT:
      gfx_state = gfx::NativeTheme::kHovered;
      break;
    case TUS_PRESSED:
      gfx_state = gfx::NativeTheme::kPressed;
      break;
    case TUS_DISABLED:
      gfx_state = gfx::NativeTheme::kDisabled;
      break;
    default:
      NOTREACHED() << "Invalid state: " << state;
      break;
  }

  switch (part) {
    case TKP_TRACK:
    case TKP_THUMBBOTTOM:
      extra->vertical = false;
      break;
    case TKP_TRACKVERT:
    case TKP_THUMBVERT:
      extra->vertical = true;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }

  return gfx_state;
}

void WebThemeEngineImpl::paintTrackbar(
    WebCanvas* canvas, int part, int state, int classic_state,
    const WebRect& rect) {
  gfx::NativeTheme::Part native_part = gfx::NativeTheme::kTrackbarTrack;
  switch (part) {
    case TKP_TRACK:
    case TKP_TRACKVERT:
      native_part = gfx::NativeTheme::kTrackbarTrack;
      break;
    case TKP_THUMBBOTTOM:
    case TKP_THUMBVERT:
      native_part = gfx::NativeTheme::kTrackbarThumb;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }

  gfx::NativeTheme::ExtraParams extra;
  gfx::NativeTheme::State native_state = WebTrackbarStateToGfx(part, state,
                                                               &extra.trackbar);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  extra.trackbar.classic_state = classic_state;
  gfx::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

void WebThemeEngineImpl::paintProgressBar(
    WebCanvas* canvas, const WebRect& barRect, const WebRect& valueRect,
    bool determinate, double animatedSeconds)
{
  gfx::Rect gfx_rect(barRect.x, barRect.y, barRect.width, barRect.height);
  gfx::NativeTheme::ExtraParams extra;
  extra.progress_bar.animated_seconds = animatedSeconds;
  extra.progress_bar.determinate = determinate;
  extra.progress_bar.value_rect_x = valueRect.x;
  extra.progress_bar.value_rect_y = valueRect.y;
  extra.progress_bar.value_rect_width = valueRect.width;
  extra.progress_bar.value_rect_height = valueRect.height;
  gfx::NativeTheme::instance()->Paint(canvas, gfx::NativeTheme::kProgressBar,
                                      gfx::NativeTheme::kNormal, gfx_rect,
                                      extra);
}

}  // namespace webkit_glue
