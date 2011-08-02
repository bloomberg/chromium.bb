// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestShellWebTheme::Control implements the generic rendering of controls
// needed by TestShellWebTheme::Engine. See the comments in that class
// header file for why this class is needed and used.
//
// This class implements a generic set of widgets using Skia. The widgets
// are optimized for testability, not a pleasing appearance.
//

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMECONTROL_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMECONTROL_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

class SkCanvas;

namespace TestShellWebTheme {

class Control {
 public:
  // This list of states mostly mirrors the list in
  // third_party/WebKit/WebCore/platform/ThemeTypes.h but is maintained
  // separately since that isn't public and also to minimize dependencies.
  // Note that the WebKit ThemeTypes seem to imply that a control can be
  // in multiple states simultaneously but WebThemeEngine only allows for
  // a single state at a time.
  //
  // Some definitions for the various states:
  //   Disabled - indicates that a control can't be modified or selected
  //              (corresponds to HTML 'disabled' attribute)
  //   ReadOnly - indicates that a control can't be modified but can be
  //              selected
  //   Normal   - the normal state of control on the page when it isn't
  //              focused or otherwise active
  //   Hot      - when the mouse is hovering over a part of the control,
  //              all the other parts are considered "hot"
  //   Hover    - when the mouse is directly over a control (the CSS
  //               :hover pseudo-class)
  //   Focused  - when the control has the keyboard focus
  //   Pressed  - when the control is being triggered (by a mousedown or
  //              a key event).
  //   Indeterminate - when set to indeterminate (only for progress bar)
  enum State {
    kUnknown_State = 0,
    kDisabled_State,
    kReadOnly_State,
    kNormal_State,
    kHot_State,
    kHover_State,
    kFocused_State,
    kPressed_State,
    kIndeterminate_State
  };

  // This list of types mostly mirrors the list in
  // third_party/WebKit/WebCore/platform/ThemeTypes.h but is maintained
  // separately since that isn't public and also to minimize dependencies.
  //
  // Note that what the user might think of as a single control can be
  // made up of multiple parts. For example, a single scroll bar contains
  // six clickable parts - two arrows, the "thumb" indicating the current
  // position on the bar, the other two parts of the bar (before and after
  // the thumb) and the "gripper" on the thumb itself.
  //
  enum Type {
    kUnknown_Type = 0,
    kTextField_Type,
    kPushButton_Type,
    kUncheckedBox_Type,
    kCheckedBox_Type,
    kIndeterminateCheckBox_Type,
    kUncheckedRadio_Type,
    kCheckedRadio_Type,
    kHorizontalScrollTrackBack_Type,
    kHorizontalScrollTrackForward_Type,
    kHorizontalScrollThumb_Type,
    kHorizontalScrollGrip_Type,
    kVerticalScrollTrackBack_Type,
    kVerticalScrollTrackForward_Type,
    kVerticalScrollThumb_Type,
    kVerticalScrollGrip_Type,
    kLeftArrow_Type,
    kRightArrow_Type,
    kUpArrow_Type,
    kDownArrow_Type,
    kHorizontalSliderTrack_Type,
    kHorizontalSliderThumb_Type,
    kDropDownButton_Type,
    kProgressBar_Type
  };

  // canvas is the canvas to draw onto, and rect gives the size of the
  // control. ctype and cstate specify the type and state of the control.
  Control(SkCanvas* canvas, const SkIRect& rect,
          Type ctype, State cstate);
  ~Control();

  // Draws the control.
  void draw();

  // Use this for TextField controls instead, because the logic
  // for drawing them is dependent on what WebKit tells us to do.
  // If draw_edges is true, draw an edge around the control. If
  // fill_content_area is true, fill the content area with the given color.
  void drawTextField(bool draw_edges, bool fill_content_area, SkColor color);

  // Use this for drawing ProgressBar controls instead, since we
  // need to know the rect to fill inside the bar.
  void drawProgressBar(const SkIRect& fill_rect);

 private:
  // Draws a box of size specified by irect, filled with the given color.
  // The box will have a border drawn in the default edge color.
  void box(const SkIRect& irect, SkColor color);


  // Draws a triangle of size specified by the three pairs of coordinates,
  // filled with the given color. The box will have an edge drawn in the
  // default edge color.
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2,
                SkColor color);

  // Draws a rectangle the size of the control with rounded corners, filled
  // with the specified color (and with a border in the default edge color).
  void roundRect(SkColor color);

  // Draws an oval the size of the control, filled with the specified color
  // and with a border in the default edge color.
  void oval(SkColor color);

  // Draws a circle centered in the control with the specified radius,
  // filled with the specified color, and with a border draw in the
  // default edge color.
  void circle(SkScalar radius, SkColor color);

  // Draws a box the size of the control, filled with the outer_color and
  // with a border in the default edge color, and then draws another box
  // indented on all four sides by the specified amounts, filled with the
  // inner color and with a border in the default edge color.
  void nested_boxes(int indent_left, int indent_top,
                    int indent_right, int indent_bottom,
                    SkColor outer_color, SkColor inner_color);

  // Draws a line between the two points in the given color.
  void line(int x0, int y0, int x1, int y1, SkColor color);

  // Draws a distinctive mark on the control for each state, so that the
  // state of the control can be determined without needing to know which
  // color is which.
  void markState();

  SkCanvas* canvas_;
  const SkIRect irect_;
  const Type type_;
  const State state_;
  const SkColor edge_color_;
  const SkColor bg_color_;
  const SkColor fg_color_;

  // The following are convenience accessors for irect_.
  const int left_;
  const int right_;
  const int top_;
  const int bottom_;
  const int width_;
  const int height_;

  DISALLOW_COPY_AND_ASSIGN(Control);
};

}  // namespace TestShellWebTheme

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBTHEMECONTROL_H_
