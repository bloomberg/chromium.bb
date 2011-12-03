// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a simple generic version of the WebKitThemeEngine,
// Since WebThemeEngine is unfortunately defined in terms of the Windows
// Theme parameters and values, we need to translate all the values into
// generic equivalents that we can more easily understand. This file does
// that translation (acting as a Facade design pattern) and then uses
// TestShellWebTheme::Control for the actual rendering of the widgets.
//

#include "webkit/tools/test_shell/test_shell_webthemeengine.h"

// Although all this code is generic, we include these headers
// to pull in the Windows #defines for the parts and states of
// the controls.
#include <vsstyle.h>
#include <windows.h>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCanvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "webkit/tools/test_shell/test_shell_webthemecontrol.h"
#include "third_party/skia/include/core/SkRect.h"

// We define this for clarity, although there really should be a DFCS_NORMAL
// in winuser.h.
namespace {
  const int kDFCSNormal = 0x0000;
}

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace TestShellWebTheme {

SkIRect webRectToSkIRect(const WebRect& web_rect) {
  SkIRect irect;
  irect.set(web_rect.x, web_rect.y, web_rect.x + web_rect.width,
            web_rect.y + web_rect.height);
  return irect;
}

void drawControl(WebCanvas* canvas, const WebRect& rect, Control::Type ctype,
                 Control::State cstate) {
  Control control(canvas, webRectToSkIRect(rect), ctype, cstate);
  control.draw();
}

void drawTextField(WebCanvas* canvas, const WebRect& rect,
                   Control::Type ctype, Control::State cstate,
                   bool draw_edges, bool fill_content_area, WebColor color) {
  Control control(canvas, webRectToSkIRect(rect), ctype, cstate);
  control.drawTextField(draw_edges, fill_content_area, color);
}

void drawProgressBar(WebCanvas* canvas,
                     Control::Type ctype, Control::State cstate,
                     const WebRect& bar_rect, const WebRect& fill_rect) {
  Control control(canvas, webRectToSkIRect(bar_rect), ctype, cstate);
  control.drawProgressBar(webRectToSkIRect(fill_rect));
}

void Engine::paintButton(WebCanvas* canvas, int part, int state,
                         int classic_state, const WebRect& rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  if (part == BP_CHECKBOX) {
    switch (state) {
      case CBS_UNCHECKEDNORMAL:
        CHECK_EQ(classic_state, kDFCSNormal);
        ctype = Control::kUncheckedBox_Type;
        cstate = Control::kNormal_State;
        break;
      case CBS_UNCHECKEDHOT:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_HOT);
        ctype = Control::kUncheckedBox_Type;
        cstate = Control::kHot_State;
        break;
      case CBS_UNCHECKEDPRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_PUSHED);
        ctype = Control::kUncheckedBox_Type;
        cstate = Control::kPressed_State;
        break;
      case CBS_UNCHECKEDDISABLED:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_INACTIVE);
        ctype = Control::kUncheckedBox_Type;
        cstate = Control::kDisabled_State;
        break;
      case CBS_CHECKEDNORMAL:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_CHECKED);
        ctype = Control::kCheckedBox_Type;
        cstate = Control::kNormal_State;
        break;
      case CBS_CHECKEDHOT:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_HOT);
        ctype = Control::kCheckedBox_Type;
        cstate = Control::kHot_State;
        break;
      case CBS_CHECKEDPRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_PUSHED);
        ctype = Control::kCheckedBox_Type;
        cstate = Control::kPressed_State;
        break;
      case CBS_CHECKEDDISABLED:
        CHECK_EQ(classic_state,
                 DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_INACTIVE);
        ctype = Control::kCheckedBox_Type;
        cstate = Control::kDisabled_State;
        break;
      case CBS_MIXEDNORMAL:
        // Classic theme can't represent mixed state checkbox. We assume
        // it's equivalent to unchecked.
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK);
        ctype = Control::kIndeterminateCheckBox_Type;
        cstate = Control::kNormal_State;
        break;
      case CBS_MIXEDHOT:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_HOT);
        ctype = Control::kIndeterminateCheckBox_Type;
        cstate = Control::kHot_State;
        break;
      case CBS_MIXEDPRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_PUSHED);
        ctype = Control::kIndeterminateCheckBox_Type;
        cstate = Control::kPressed_State;
        break;
      case CBS_MIXEDDISABLED:
        CHECK_EQ(classic_state, DFCS_BUTTONCHECK | DFCS_INACTIVE);
        ctype = Control::kIndeterminateCheckBox_Type;
        cstate = Control::kDisabled_State;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (BP_RADIOBUTTON == part) {
    switch (state) {
      case RBS_UNCHECKEDNORMAL:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO);
        ctype = Control::kUncheckedRadio_Type;
        cstate = Control::kNormal_State;
        break;
      case RBS_UNCHECKEDHOT:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_HOT);
        ctype = Control::kUncheckedRadio_Type;
        cstate = Control::kHot_State;
        break;
      case RBS_UNCHECKEDPRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_PUSHED);
        ctype = Control::kUncheckedRadio_Type;
        cstate = Control::kPressed_State;
        break;
      case RBS_UNCHECKEDDISABLED:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_INACTIVE);
        ctype = Control::kUncheckedRadio_Type;
        cstate = Control::kDisabled_State;
        break;
      case RBS_CHECKEDNORMAL:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_CHECKED);
        ctype = Control::kCheckedRadio_Type;
        cstate = Control::kNormal_State;
        break;
      case RBS_CHECKEDHOT:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_HOT);
        ctype = Control::kCheckedRadio_Type;
        cstate = Control::kHot_State;
        break;
      case RBS_CHECKEDPRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_PUSHED);
        ctype = Control::kCheckedRadio_Type;
        cstate = Control::kPressed_State;
        break;
      case RBS_CHECKEDDISABLED:
        CHECK_EQ(classic_state,
                 DFCS_BUTTONRADIO | DFCS_CHECKED | DFCS_INACTIVE);
        ctype = Control::kCheckedRadio_Type;
        cstate = Control::kDisabled_State;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (BP_PUSHBUTTON == part) {
    switch (state) {
      case PBS_NORMAL:
        CHECK_EQ(classic_state, DFCS_BUTTONPUSH);
        ctype = Control::kPushButton_Type;
        cstate = Control::kNormal_State;
        break;
      case PBS_HOT:
        CHECK_EQ(classic_state, DFCS_BUTTONPUSH | DFCS_HOT);
        ctype = Control::kPushButton_Type;
        cstate = Control::kHot_State;
        break;
      case PBS_PRESSED:
        CHECK_EQ(classic_state, DFCS_BUTTONPUSH | DFCS_PUSHED);
        ctype = Control::kPushButton_Type;
        cstate = Control::kPressed_State;
        break;
      case PBS_DISABLED:
        CHECK_EQ(classic_state, DFCS_BUTTONPUSH | DFCS_INACTIVE);
        ctype = Control::kPushButton_Type;
        cstate = Control::kDisabled_State;
        break;
      case PBS_DEFAULTED:
        CHECK_EQ(classic_state, DFCS_BUTTONPUSH);
        ctype = Control::kPushButton_Type;
        cstate = Control::kFocused_State;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else {
    NOTREACHED();
  }

  drawControl(canvas, rect, ctype, cstate);
}


void Engine::paintMenuList(WebCanvas* canvas, int part, int state,
                           int classic_state, const WebRect& rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  if (CP_DROPDOWNBUTTON == part) {
    ctype = Control::kDropDownButton_Type;
    switch (state) {
      case CBXS_NORMAL:
        CHECK_EQ(classic_state, DFCS_MENUARROW);
        cstate = Control::kNormal_State;
        break;
      case CBXS_HOT:
        CHECK_EQ(classic_state, DFCS_MENUARROW | DFCS_HOT);
        cstate = Control::kHover_State;
        break;
      case CBXS_PRESSED:
        CHECK_EQ(classic_state, DFCS_MENUARROW | DFCS_PUSHED);
        cstate = Control::kPressed_State;
        break;
      case CBXS_DISABLED:
        CHECK_EQ(classic_state, DFCS_MENUARROW | DFCS_INACTIVE);
        cstate = Control::kDisabled_State;
        break;
      default:
        CHECK(false);
        break;
    }
  } else {
    CHECK(false);
  }

  drawControl(canvas, rect, ctype, cstate);
}

void Engine::paintScrollbarArrow(WebCanvas* canvas, int state,
                                 int classic_state, const WebRect& rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  switch (state) {
    case ABS_UPNORMAL:
      CHECK_EQ(classic_state, DFCS_SCROLLUP);
      ctype = Control::kUpArrow_Type;
      cstate = Control::kNormal_State;
      break;
    case ABS_DOWNNORMAL:
      CHECK_EQ(classic_state, DFCS_SCROLLDOWN);
      ctype = Control::kDownArrow_Type;
      cstate = Control::kNormal_State;
      break;
    case ABS_LEFTNORMAL:
      CHECK_EQ(classic_state, DFCS_SCROLLLEFT);
      ctype = Control::kLeftArrow_Type;
      cstate = Control::kNormal_State;
      break;
    case ABS_RIGHTNORMAL:
      CHECK_EQ(classic_state, DFCS_SCROLLRIGHT);
      ctype = Control::kRightArrow_Type;
      cstate = Control::kNormal_State;
      break;
    case ABS_UPHOT:
      CHECK_EQ(classic_state, DFCS_SCROLLUP | DFCS_HOT);
      ctype = Control::kUpArrow_Type;
      cstate = Control::kHot_State;
      break;
    case ABS_DOWNHOT:
      CHECK_EQ(classic_state, DFCS_SCROLLDOWN | DFCS_HOT);
      ctype = Control::kDownArrow_Type;
      cstate = Control::kHot_State;
      break;
    case ABS_LEFTHOT:
      CHECK_EQ(classic_state, DFCS_SCROLLLEFT | DFCS_HOT);
      ctype = Control::kLeftArrow_Type;
      cstate = Control::kHot_State;
      break;
    case ABS_RIGHTHOT:
      CHECK_EQ(classic_state, DFCS_SCROLLRIGHT | DFCS_HOT);
      ctype = Control::kRightArrow_Type;
      cstate = Control::kHot_State;
      break;
    case ABS_UPHOVER:
      CHECK_EQ(classic_state, DFCS_SCROLLUP);
      ctype = Control::kUpArrow_Type;
      cstate = Control::kHover_State;
      break;
    case ABS_DOWNHOVER:
      CHECK_EQ(classic_state, DFCS_SCROLLDOWN);
      ctype = Control::kDownArrow_Type;
      cstate = Control::kHover_State;
      break;
    case ABS_LEFTHOVER:
      CHECK_EQ(classic_state, DFCS_SCROLLLEFT);
      ctype = Control::kLeftArrow_Type;
      cstate = Control::kHover_State;
      break;
    case ABS_RIGHTHOVER:
      CHECK_EQ(classic_state, DFCS_SCROLLRIGHT);
      ctype = Control::kRightArrow_Type;
      cstate = Control::kHover_State;
      break;
    case ABS_UPPRESSED:
      CHECK_EQ(classic_state, DFCS_SCROLLUP | DFCS_PUSHED | DFCS_FLAT);
      ctype = Control::kUpArrow_Type;
      cstate = Control::kPressed_State;
      break;
    case ABS_DOWNPRESSED:
      CHECK_EQ(classic_state, DFCS_SCROLLDOWN | DFCS_PUSHED | DFCS_FLAT);
      ctype = Control::kDownArrow_Type;
      cstate = Control::kPressed_State;
      break;
    case ABS_LEFTPRESSED:
      CHECK_EQ(classic_state, DFCS_SCROLLLEFT | DFCS_PUSHED | DFCS_FLAT);
      ctype = Control::kLeftArrow_Type;
      cstate = Control::kPressed_State;
      break;
    case ABS_RIGHTPRESSED:
      CHECK_EQ(classic_state, DFCS_SCROLLRIGHT | DFCS_PUSHED | DFCS_FLAT);
      ctype = Control::kRightArrow_Type;
      cstate = Control::kPressed_State;
      break;
    case ABS_UPDISABLED:
      CHECK_EQ(classic_state, DFCS_SCROLLUP | DFCS_INACTIVE);
      ctype = Control::kUpArrow_Type;
      cstate = Control::kDisabled_State;
      break;
    case ABS_DOWNDISABLED:
      CHECK_EQ(classic_state, DFCS_SCROLLDOWN | DFCS_INACTIVE);
      ctype = Control::kDownArrow_Type;
      cstate = Control::kDisabled_State;
      break;
    case ABS_LEFTDISABLED:
      CHECK_EQ(classic_state, DFCS_SCROLLLEFT | DFCS_INACTIVE);
      ctype = Control::kLeftArrow_Type;
      cstate = Control::kDisabled_State;
      break;
    case ABS_RIGHTDISABLED:
      CHECK_EQ(classic_state, DFCS_SCROLLRIGHT | DFCS_INACTIVE);
      ctype = Control::kRightArrow_Type;
      cstate = Control::kDisabled_State;
      break;
    default:
      NOTREACHED();
      break;
  }

  drawControl(canvas, rect, ctype, cstate);
}

void Engine::paintScrollbarThumb(WebCanvas* canvas, int part, int state,
                                 int classic_state, const WebRect& rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  switch (part) {
    case SBP_THUMBBTNHORZ:
      ctype = Control::kHorizontalScrollThumb_Type;
      break;
    case SBP_THUMBBTNVERT:
      ctype = Control::kVerticalScrollThumb_Type;
      break;
    case SBP_GRIPPERHORZ:
      ctype = Control::kHorizontalScrollGrip_Type;
      break;
    case SBP_GRIPPERVERT:
      ctype = Control::kVerticalScrollGrip_Type;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (state) {
    case SCRBS_NORMAL:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kNormal_State;
      break;
    case SCRBS_HOT:
      CHECK_EQ(classic_state, DFCS_HOT);
      cstate = Control::kHot_State;
      break;
    case SCRBS_HOVER:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kHover_State;
      break;
    case SCRBS_PRESSED:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kPressed_State;
      break;
    case SCRBS_DISABLED:
      NOTREACHED();  // This should never happen in practice.
      break;
    default:
      NOTREACHED();
      break;
  }

  drawControl(canvas, rect, ctype, cstate);
}

void Engine::paintScrollbarTrack(WebCanvas* canvas, int part, int state,
                                 int classic_state, const WebRect& rect,
                                 const WebRect& align_rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  switch (part) {
    case SBP_UPPERTRACKHORZ:
      ctype = Control::kHorizontalScrollTrackBack_Type;
      break;
    case SBP_LOWERTRACKHORZ:
      ctype = Control::kHorizontalScrollTrackForward_Type;
      break;
    case SBP_UPPERTRACKVERT:
      ctype = Control::kVerticalScrollTrackBack_Type;
      break;
    case SBP_LOWERTRACKVERT:
      ctype = Control::kVerticalScrollTrackForward_Type;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (state) {
    case SCRBS_NORMAL:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kNormal_State;
      break;
    case SCRBS_HOT:
      NOTREACHED();  // This should never happen in practice.
      break;
    case SCRBS_HOVER:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kHover_State;
      break;
    case SCRBS_PRESSED:
      NOTREACHED();  // This should never happen in practice.
      break;
    case SCRBS_DISABLED:
      CHECK_EQ(classic_state, DFCS_INACTIVE);
      cstate = Control::kDisabled_State;
      break;
    default:
      CHECK(false);
      break;
  }

  drawControl(canvas, rect, ctype, cstate);
}

void Engine::paintTextField(WebCanvas* canvas, int part, int state,
                            int classic_state, const WebRect& rect,
                            WebColor color, bool fill_content_area,
                            bool draw_edges) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  CHECK_EQ(EP_EDITTEXT, part);
  ctype = Control::kTextField_Type;

  switch (state) {
    case ETS_NORMAL:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kNormal_State;
      break;
    case ETS_HOT:
      CHECK_EQ(classic_state, DFCS_HOT);
      cstate = Control::kHot_State;
      break;
    case ETS_DISABLED:
      CHECK_EQ(classic_state, DFCS_INACTIVE);
      cstate = Control::kDisabled_State;
      break;
    case ETS_SELECTED:
      CHECK_EQ(classic_state, DFCS_PUSHED);
      cstate = Control::kPressed_State;
      break;
    case ETS_FOCUSED:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kFocused_State;
      break;
    case ETS_READONLY:
      CHECK_EQ(classic_state, kDFCSNormal);
      cstate = Control::kReadOnly_State;
      break;
    default:
      NOTREACHED();
      break;
  }

  drawTextField(canvas, rect, ctype, cstate, draw_edges, fill_content_area,
                color);
}

void Engine::paintTrackbar(WebCanvas* canvas, int part, int state,
                           int classic_state, const WebRect& rect) {
  Control::Type ctype = Control::kUnknown_Type;
  Control::State cstate = Control::kUnknown_State;

  if (TKP_THUMBBOTTOM == part) {
    ctype = Control::kHorizontalSliderThumb_Type;
    switch (state) {
      case TUS_NORMAL:
        CHECK_EQ(classic_state, kDFCSNormal);
        cstate = Control::kNormal_State;
        break;
      case TUS_HOT:
        CHECK_EQ(classic_state, DFCS_HOT);
        cstate = Control::kHot_State;
        break;
      case TUS_DISABLED:
        CHECK_EQ(classic_state, DFCS_INACTIVE);
        cstate = Control::kDisabled_State;
        break;
      case TUS_PRESSED:
        CHECK_EQ(classic_state, DFCS_PUSHED);
        cstate = Control::kPressed_State;
        break;
      default:
        NOTREACHED();
        break;
      }
  } else if (TKP_TRACK == part) {
    ctype = Control::kHorizontalSliderTrack_Type;
    CHECK_EQ(part, TUS_NORMAL);
    CHECK_EQ(classic_state, kDFCSNormal);
    cstate = Control::kNormal_State;
  } else {
    NOTREACHED();
  }

  drawControl(canvas, rect, ctype, cstate);
}


void Engine::paintProgressBar(WebKit::WebCanvas* canvas,
                              const WebKit::WebRect& barRect,
                              const WebKit::WebRect& valueRect,
                              bool determinate, double) {
  Control::Type ctype = Control::kProgressBar_Type;
  Control::State cstate =
      determinate ? Control::kNormal_State : Control::kIndeterminate_State;
  drawProgressBar(canvas, ctype, cstate, barRect, valueRect);
}

}  // namespace TestShellWebTheme
