// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_win.h"

#include <vsstyle.h>  // To convert to ui::NativeTheme::State

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "ui/base/win/dpi.h"
#include "ui/native_theme/native_theme.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;
using WebKit::WebSize;

namespace webkit_glue {

static RECT WebRectToRECT(const WebRect& rect) {
  RECT result;
  result.left = rect.x;
  result.top = rect.y;
  result.right = rect.x + rect.width;
  result.bottom = rect.y + rect.height;
  return result;
}

static ui::NativeTheme::State WebButtonStateToGfx(
    int part, int state, ui::NativeTheme::ButtonExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  // Native buttons have a different focus style.
  extra->is_focused = false;
  extra->has_border = false;
  extra->background_color = ui::NativeTheme::instance()->GetSystemColor(
      ui::NativeTheme::kColorId_TextButtonBackgroundColor);

  if (part == BP_PUSHBUTTON) {
    switch (state) {
      case PBS_NORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_HOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_PRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_DISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case PBS_DEFAULTED:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      case PBS_DEFAULTED_ANIMATING:
        gfx_state = ui::NativeTheme::kNormal;
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
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_UNCHECKEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDNORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case RBS_CHECKEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
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
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_UNCHECKEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDNORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_CHECKEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = true;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_MIXEDNORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_MIXEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = true;
        extra->is_default = false;
        break;
      case CBS_IMPLICITNORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_IMPLICITDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDNORMAL:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDHOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDPRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CBS_EXCLUDEDDISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
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
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case GBS_DISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
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
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_HOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_PRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_DISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLS_DEFAULTED:
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = true;
        break;
      case CMDLS_DEFAULTED_ANIMATING:
        gfx_state = ui::NativeTheme::kNormal;
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
        gfx_state = ui::NativeTheme::kNormal;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_HOT:
        gfx_state = ui::NativeTheme::kHovered;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_PRESSED:
        gfx_state = ui::NativeTheme::kPressed;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_DISABLED:
        gfx_state = ui::NativeTheme::kDisabled;
        extra->checked = false;
        extra->indeterminate = false;
        extra->is_default = false;
        break;
      case CMDLGS_DEFAULTED:
        gfx_state = ui::NativeTheme::kNormal;
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
  ui::NativeTheme::Part native_part = ui::NativeTheme::kPushButton;
  switch (part) {
    case BP_PUSHBUTTON:
      native_part = ui::NativeTheme::kPushButton;
      break;
    case BP_CHECKBOX:
      native_part = ui::NativeTheme::kCheckbox;
      break;
    case BP_RADIOBUTTON:
      native_part = ui::NativeTheme::kRadio;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebButtonStateToGfx(part, state,
                                                             &extra.button);
  extra.button.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebListMenuStateToGfx(int part, int state) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;

  switch (part) {
    case CP_DROPDOWNBUTTON:
      switch (state) {
        case CBXS_NORMAL:
          gfx_state = ui::NativeTheme::kNormal;
          break;
        case CBXS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          break;
        case CBXS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          break;
        case CBXS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
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
  ui::NativeTheme::Part native_part = ui::NativeTheme::kMenuList;
  switch (part) {
    case CP_DROPDOWNBUTTON:
      native_part = ui::NativeTheme::kMenuList;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }
  ui::NativeTheme::State native_state = WebListMenuStateToGfx(part, state);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::ExtraParams extra;
  extra.menu_list.classic_state = classic_state;
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebScrollbarArrowStateToGfx(
    int state, ui::NativeTheme::Part* part,
    ui::NativeTheme::ScrollbarArrowExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (state) {
    case ABS_UPNORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      *part = ui::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPHOT:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPPRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      *part = ui::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPDISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
      *part = ui::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNNORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      *part = ui::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNHOT:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNPRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      *part = ui::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_DOWNDISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
      *part = ui::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTNORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      *part = ui::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTHOT:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTPRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      *part = ui::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_LEFTDISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
      *part = ui::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTNORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      *part = ui::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTHOT:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTPRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      *part = ui::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_RIGHTDISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
      *part = ui::NativeTheme::kScrollbarRightArrow;
      extra->is_hovering = false;
      break;
    case ABS_UPHOVER:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarUpArrow;
      extra->is_hovering = true;
      break;
    case ABS_DOWNHOVER:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarDownArrow;
      extra->is_hovering = true;
      break;
    case ABS_LEFTHOVER:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarLeftArrow;
      extra->is_hovering = true;
      break;
    case ABS_RIGHTHOVER:
      gfx_state = ui::NativeTheme::kHovered;
      *part = ui::NativeTheme::kScrollbarRightArrow;
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
  ui::NativeTheme::Part native_part;
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebScrollbarArrowStateToGfx(
      state, &native_part, &extra.scrollbar_arrow);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebScrollbarThumbStateToGfx(
    int state, ui::NativeTheme::ScrollbarThumbExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (state) {
    case SCRBS_NORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      extra->is_hovering = false;
      break;
    case SCRBS_HOVER:
      gfx_state = ui::NativeTheme::kHovered;
      extra->is_hovering = true;
      break;
    case SCRBS_HOT:
      gfx_state = ui::NativeTheme::kHovered;
      extra->is_hovering = false;
      break;
    case SCRBS_PRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      extra->is_hovering = false;
      break;
    case SCRBS_DISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
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
  ui::NativeTheme::Part native_part;
  if (part == SBP_THUMBBTNHORZ) {
    native_part = ui::NativeTheme::kScrollbarHorizontalThumb;
  } else if (part == SBP_THUMBBTNVERT) {
    native_part = ui::NativeTheme::kScrollbarVerticalThumb;
  } else if (part == SBP_GRIPPERHORZ) {
    native_part = ui::NativeTheme::kScrollbarHorizontalGripper;
  } else if (part == SBP_GRIPPERVERT) {
    native_part = ui::NativeTheme::kScrollbarVerticalGripper;
  } else {
      NOTREACHED() << "Invalid part: " << part;
  }

  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebScrollbarThumbStateToGfx(
      state, &extra.scrollbar_thumb);

  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebScrollbarTrackStateToGfx(
    int part, int state, ui::NativeTheme::Part* gfx_part,
    ui::NativeTheme::ScrollbarTrackExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (part) {
    case SBP_LOWERTRACKHORZ:
      switch (state) {
        case SCRBS_NORMAL:
          gfx_state = ui::NativeTheme::kNormal;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
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
          gfx_state = ui::NativeTheme::kNormal;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
          *gfx_part = ui::NativeTheme::kScrollbarHorizontalTrack;
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
          gfx_state = ui::NativeTheme::kNormal;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = false;
          break;
        case SCRBS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
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
          gfx_state = ui::NativeTheme::kNormal;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_HOVER:
        case SCRBS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
          extra->is_upper = true;
          break;
        case SCRBS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
          *gfx_part = ui::NativeTheme::kScrollbarVerticalTrack;
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
  ui::NativeTheme::Part native_part;
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebScrollbarTrackStateToGfx(
      part, state, &native_part, &extra.scrollbar_track);
  extra.scrollbar_track.classic_state = classic_state;
  extra.scrollbar_track.track_x = align_rect.x;
  extra.scrollbar_track.track_y = align_rect.y;
  extra.scrollbar_track.track_width = align_rect.width;
  extra.scrollbar_track.track_height = align_rect.height;

  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebSpinButtonStateToGfx(
    int part, int state, ui::NativeTheme::InnerSpinButtonExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (part) {
    case SPNP_UP:
      switch (state) {
        case UPS_NORMAL:
          gfx_state = ui::NativeTheme::kNormal;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          extra->spin_up = true;
          extra->read_only = false;
          break;
        case UPS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
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
          gfx_state = ui::NativeTheme::kNormal;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_PRESSED:
          gfx_state = ui::NativeTheme::kPressed;
          extra->spin_up = false;
          extra->read_only = false;
          break;
        case DNS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
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
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebSpinButtonStateToGfx(
      part, state, &extra.inner_spin);
  extra.inner_spin.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  ui::NativeTheme::instance()->Paint(canvas,
                                      ui::NativeTheme::kInnerSpinButton,
                                      native_state,
                                      gfx_rect,
                                      extra);
}

static ui::NativeTheme::State WebTextFieldStateToGfx(
    int part, int state, ui::NativeTheme::TextFieldExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (part) {
    case EP_EDITTEXT:
      switch (state) {
        case ETS_NORMAL:
          gfx_state = ui::NativeTheme::kNormal;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_HOT:
          gfx_state = ui::NativeTheme::kHovered;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_SELECTED:
          gfx_state = ui::NativeTheme::kPressed;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_DISABLED:
          gfx_state = ui::NativeTheme::kDisabled;
          extra->is_read_only = false;
          extra->is_focused = false;
          break;
        case ETS_FOCUSED:
          gfx_state = ui::NativeTheme::kNormal;
          extra->is_read_only = false;
          extra->is_focused = true;
          break;
        case ETS_READONLY:
          gfx_state = ui::NativeTheme::kNormal;
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
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebTextFieldStateToGfx(
      part, state, &extra.text_field);
  extra.text_field.fill_content_area = fill_content_area;
  extra.text_field.draw_edges = draw_edges;
  extra.text_field.background_color = color;
  extra.text_field.classic_state = classic_state;
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);

  ui::NativeTheme::instance()->Paint(canvas,
      ui::NativeTheme::kTextField, native_state, gfx_rect, extra);
}

static ui::NativeTheme::State WebTrackbarStateToGfx(
    int part,
    int state,
    ui::NativeTheme::TrackbarExtraParams* extra) {
  ui::NativeTheme::State gfx_state = ui::NativeTheme::kNormal;
  switch (state) {
    case TUS_NORMAL:
      gfx_state = ui::NativeTheme::kNormal;
      break;
    case TUS_HOT:
      gfx_state = ui::NativeTheme::kHovered;
      break;
    case TUS_PRESSED:
      gfx_state = ui::NativeTheme::kPressed;
      break;
    case TUS_DISABLED:
      gfx_state = ui::NativeTheme::kDisabled;
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
  ui::NativeTheme::Part native_part = ui::NativeTheme::kTrackbarTrack;
  switch (part) {
    case TKP_TRACK:
    case TKP_TRACKVERT:
      native_part = ui::NativeTheme::kTrackbarTrack;
      break;
    case TKP_THUMBBOTTOM:
    case TKP_THUMBVERT:
      native_part = ui::NativeTheme::kTrackbarThumb;
      break;
    default:
      NOTREACHED() << "Invalid part: " << part;
      break;
  }

  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::State native_state = WebTrackbarStateToGfx(part, state,
                                                               &extra.trackbar);
  gfx::Rect gfx_rect(rect.x, rect.y, rect.width, rect.height);
  extra.trackbar.classic_state = classic_state;
  ui::NativeTheme::instance()->Paint(canvas, native_part,
                                      native_state, gfx_rect, extra);
}

void WebThemeEngineImpl::paintProgressBar(
    WebCanvas* canvas, const WebRect& barRect, const WebRect& valueRect,
    bool determinate, double animatedSeconds)
{
  gfx::Rect gfx_rect(barRect.x, barRect.y, barRect.width, barRect.height);
  ui::NativeTheme::ExtraParams extra;
  extra.progress_bar.animated_seconds = animatedSeconds;
  extra.progress_bar.determinate = determinate;
  extra.progress_bar.value_rect_x = valueRect.x;
  extra.progress_bar.value_rect_y = valueRect.y;
  extra.progress_bar.value_rect_width = valueRect.width;
  extra.progress_bar.value_rect_height = valueRect.height;
  ui::NativeTheme::instance()->Paint(canvas, ui::NativeTheme::kProgressBar,
                                      ui::NativeTheme::kNormal, gfx_rect,
                                      extra);
}

WebSize WebThemeEngineImpl::getSize(int part) {
  switch (part) {
    case SBP_ARROWBTN: {
      gfx::Size size = ui::NativeTheme::instance()->GetPartSize(
          ui::NativeTheme::kScrollbarUpArrow,
          ui::NativeTheme::kNormal,
          ui::NativeTheme::ExtraParams());
      // GetPartSize returns a size of (0, 0) when not using a themed style
      // (i.e. Windows Classic).  Returning a non-zero size in this context
      // creates repaint conflicts, particularly in the window titlebar area
      // which significantly degrades performance.  Fallback to using a system
      // metric if required.
      if (size.width() == 0) {
        int width = static_cast<int>(GetSystemMetrics(SM_CXVSCROLL) /
            ui::win::GetDeviceScaleFactor());
        size = gfx::Size(width, width);
      }
      return WebSize(size.width(), size.height());
    }
    default:
      NOTREACHED() << "Unhandled part: " << part;
  }
  return WebSize();
}

}  // namespace webkit_glue
