// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LABEL_H_
#define UI_VIEWS_CONTROLS_LABEL_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/views/view.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// Label class
//
// A label is a view subclass that can display a string.
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Label : public View {
 public:
  enum Alignment { ALIGN_LEFT = 0,
                   ALIGN_CENTER,
                   ALIGN_RIGHT };

  // The following enum is used to indicate whether using the Chrome UI's
  // directionality as the label's directionality, or auto-detecting the label's
  // directionality.
  //
  // If the label text originates from the Chrome UI, we should use the Chrome
  // UI's directionality as the label's directionality.
  //
  // If the text originates from a web page, its directionality is determined
  // based on its first character with strong directionality, disregarding what
  // directionality the Chrome UI is.
  enum DirectionalityMode {
    USE_UI_DIRECTIONALITY = 0,
    AUTO_DETECT_DIRECTIONALITY
  };

  // The view class name.
  static const char kViewClassName[];

  // The padding for the focus border when rendering focused text.
  static const int kFocusBorderPadding;

  Label();
  explicit Label(const string16& text);
  Label(const string16& text, const gfx::Font& font);
  virtual ~Label();

  // Sets the font.
  virtual void SetFont(const gfx::Font& font);

  // Sets the label text.
  void SetText(const string16& text);

  // Sets the label text to |email|.  Emails have a custom eliding algorithm.
  void SetEmail(const string16& email);

  // Sets URL Value - text_ is set to spec().
  void SetURL(const GURL& url);

  // Returns the font used by this label.
  gfx::Font font() const { return font_; }

  // Returns the label text.
  string16 text() const { return text_; };

  // Enables or disables auto-color-readability (enabled by default).  If this
  // is enabled, then calls to set any foreground or background color will
  // trigger an automatic mapper that uses color_utils::GetReadableColor() to
  // ensure that the foreground colors are readable over the background color.
  void SetAutoColorReadabilityEnabled(bool enabled);

  // Sets the color.  This will automatically force the color to be readable
  // over the current background color.
  virtual void SetEnabledColor(SkColor color);
  void SetDisabledColor(SkColor color);

  SkColor enabled_color() const { return actual_enabled_color_; }

  // Sets the background color.  This won't be explicitly drawn, but the label
  // will force the text color to be readable over it.
  void SetBackgroundColor(SkColor color);

  // Enables a drop shadow underneath the text.
  void SetShadowColors(SkColor enabled_color, SkColor disabled_color);

  // Sets the drop shadow's offset from the text.
  void SetShadowOffset(int x, int y);

  // Disables shadows.
  void ClearEmbellishing();

  // Sets horizontal alignment. If the locale is RTL, and the directionality
  // mode is USE_UI_DIRECTIONALITY, the alignment is flipped around.
  //
  // Caveat: for labels originating from a web page, the directionality mode
  // should be reset to AUTO_DETECT_DIRECTIONALITY before the horizontal
  // alignment is set. Otherwise, the label's alignment specified as a parameter
  // will be flipped in RTL locales.
  void SetHorizontalAlignment(Alignment alignment);

  Alignment horizontal_alignment() const { return horiz_alignment_; }

  // Sets the directionality mode. The directionality mode is initialized to
  // USE_UI_DIRECTIONALITY when the label is constructed. USE_UI_DIRECTIONALITY
  // applies to every label that originates from the Chrome UI. However, if the
  // label originates from a web page, its directionality is auto-detected.
  void set_directionality_mode(DirectionalityMode mode) {
    directionality_mode_ = mode;
  }

  DirectionalityMode directionality_mode() const {
    return directionality_mode_;
  }

  // Sets whether the label text can wrap on multiple lines.
  // Default is false.
  void SetMultiLine(bool multi_line);

  // Returns whether the label text can wrap on multiple lines.
  bool is_multi_line() const { return is_multi_line_; }

  // Sets whether the label text can be split on words.
  // Default is false. This only works when is_multi_line is true.
  void SetAllowCharacterBreak(bool allow_character_break);

  // Sets whether the label text should be elided in the middle (if necessary).
  // The default is to elide at the end.
  // NOTE: This is not supported for multi-line strings.
  void SetElideInMiddle(bool elide_in_middle);

  // Sets the tooltip text.  Default behavior for a label (single-line) is to
  // show the full text if it is wider than its bounds.  Calling this overrides
  // the default behavior and lets you set a custom tooltip.  To revert to
  // default behavior, call this with an empty string.
  void SetTooltipText(const string16& tooltip_text);

  // The background color to use when the mouse is over the label. Label
  // takes ownership of the Background.
  void SetMouseOverBackground(Background* background);
  const Background* GetMouseOverBackground() const;

  // Resizes the label so its width is set to the width of the longest line and
  // its height deduced accordingly.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |max_width| is the maximum width that will be used (longer lines will be
  // wrapped).  If 0, no maximum width is enforced.
  void SizeToFit(int max_width);

  // Gets/sets the flag to determine whether the label should be collapsed when
  // it's hidden (not visible). If this flag is true, the label will return a
  // preferred size of (0, 0) when it's not visible.
  void set_collapse_when_hidden(bool value) { collapse_when_hidden_ = value; }
  bool collapse_when_hidden() const { return collapse_when_hidden_; }

  // Gets/set whether or not this label is to be painted as a focused element.
  void set_paint_as_focused(bool paint_as_focused) {
    paint_as_focused_ = paint_as_focused;
  }
  bool paint_as_focused() const { return paint_as_focused_; }

  void SetHasFocusBorder(bool has_focus_border);

  // Overridden from View:
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  // Overridden to compute the size required to display this label.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  // Returns the height necessary to display this label with the provided width.
  // This method is used to layout multi-line labels. It is equivalent to
  // GetPreferredSize().height() if the receiver is not multi-line.
  virtual int GetHeightForWidth(int w) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
  // Mouse enter/exit are overridden to render mouse over background color.
  // These invoke SetContainsMouse as necessary.
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  // Gets the tooltip text for labels that are wider than their bounds, except
  // when the label is multiline, in which case it just returns false (no
  // tooltip).  If a custom tooltip has been specified with SetTooltipText()
  // it is returned instead.
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;

 protected:
  // Called by Paint to paint the text.  Override this to change how
  // text is painted.
  virtual void PaintText(gfx::Canvas* canvas,
                         const string16& text,
                         const gfx::Rect& text_bounds,
                         int flags);

  void invalidate_text_size() { text_size_valid_ = false; }

  virtual gfx::Size GetTextSize() const;

  SkColor disabled_color() const { return actual_disabled_color_; }

  // Overridden from View:
  // Overridden to dirty our text bounds if we're multi-line.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  // If the mouse is over the label, and a mouse over background has been
  // specified, its used. Otherwise super's implementation is invoked.
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

 private:
  // These tests call CalculateDrawStringParams in order to verify the
  // calculations done for drawing text.
  FRIEND_TEST_ALL_PREFIXES(LabelTest, DrawSingleLineString);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, DrawMultiLineString);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, DrawSingleLineStringInRTL);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, DrawMultiLineStringInRTL);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, AutoDetectDirectionality);

  // Calls ComputeDrawStringFlags().
  FRIEND_TEST_ALL_PREFIXES(LabelTest, DisableSubpixelRendering);

  static gfx::Font GetDefaultFont();

  void Init(const string16& text, const gfx::Font& font);

  void RecalculateColors();

  // If the mouse is over the text, SetContainsMouse(true) is invoked, otherwise
  // SetContainsMouse(false) is invoked.
  void UpdateContainsMouse(const MouseEvent& event);

  // Updates whether the mouse is contained in the Label. If the new value
  // differs from the current value, and a mouse over background is specified,
  // SchedulePaint is invoked.
  void SetContainsMouse(bool contains_mouse);

  // Returns where the text is drawn, in the receivers coordinate system.
  gfx::Rect GetTextBounds() const;

  int ComputeDrawStringFlags() const;

  gfx::Rect GetAvailableRect() const;

  // Returns parameters to be used for the DrawString call.
  void CalculateDrawStringParams(string16* paint_text,
                                 gfx::Rect* text_bounds,
                                 int* flags) const;

  string16 text_;
  GURL url_;
  gfx::Font font_;
  SkColor requested_enabled_color_;
  SkColor actual_enabled_color_;
  SkColor requested_disabled_color_;
  SkColor actual_disabled_color_;
  SkColor background_color_;
  bool auto_color_readability_;
  mutable gfx::Size text_size_;
  mutable bool text_size_valid_;
  bool is_multi_line_;
  bool allow_character_break_;
  bool elide_in_middle_;
  bool is_email_;
  Alignment horiz_alignment_;
  string16 tooltip_text_;
  // Whether the mouse is over this label.
  bool contains_mouse_;
  scoped_ptr<Background> mouse_over_background_;
  // Whether to collapse the label when it's not visible.
  bool collapse_when_hidden_;
  // The following member variable is used to control whether the
  // directionality is auto-detected based on first strong directionality
  // character or is determined by chrome UI's locale.
  DirectionalityMode directionality_mode_;
  // When embedded in a larger control that is focusable, setting this flag
  // allows this view to be painted as focused even when it is itself not.
  bool paint_as_focused_;
  // When embedded in a larger control that is focusable, setting this flag
  // allows this view to reserve space for a focus border that it otherwise
  // might not have because it is not itself focusable.
  bool has_focus_border_;

  // Colors for shadow.
  SkColor enabled_shadow_color_;
  SkColor disabled_shadow_color_;

  // Space between text and shadow.
  gfx::Point shadow_offset_;

  // Should a shadow be drawn behind the text?
  bool has_shadow_;


  DISALLOW_COPY_AND_ASSIGN(Label);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LABEL_H_
