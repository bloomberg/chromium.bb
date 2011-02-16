// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
#pragma once

#include <string>

// TODO(avi): remove when not needed
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "views/border.h"
#include "views/controls/button/custom_button.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder
//
//  A Border subclass that paints a TextButton's background layer -
//  basically the button frame in the hot/pushed states.
//
// Note that this type of button is not focusable by default and will not be
// part of the focus chain.  Call SetFocusable(true) to make it part of the
// focus chain.
//
////////////////////////////////////////////////////////////////////////////////
class TextButtonBorder : public Border {
 public:
  TextButtonBorder();
  virtual ~TextButtonBorder();

  // Render the background for the provided view
  virtual void Paint(const View& view, gfx::Canvas* canvas) const;

  // Returns the insets for the border.
  virtual void GetInsets(gfx::Insets* insets) const;

 protected:
  // Images
  struct MBBImageSet {
    SkBitmap* top_left;
    SkBitmap* top;
    SkBitmap* top_right;
    SkBitmap* left;
    SkBitmap* center;
    SkBitmap* right;
    SkBitmap* bottom_left;
    SkBitmap* bottom;
    SkBitmap* bottom_right;
  };
  MBBImageSet hot_set_;
  MBBImageSet pushed_set_;

  virtual void Paint(const View& view, gfx::Canvas* canvas,
      const MBBImageSet& set) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextButtonBorder);
};


////////////////////////////////////////////////////////////////////////////////
//
// TextButton
//
//  A button which displays text and/or and icon that can be changed in
//  response to actions. TextButton reserves space for the largest string
//  passed to SetText. To reset the cached max size invoke ClearMaxTextSize.
//
////////////////////////////////////////////////////////////////////////////////
class TextButton : public CustomButton {
 public:
  // The menu button's class name.
  static const char kViewClassName[];

  // Enumeration of how the prefix ('&') character is processed. The default
  // is |PREFIX_NONE|.
  enum PrefixType {
    // No special processing is done.
    PREFIX_NONE,

    // The character following the prefix character is not rendered specially.
    PREFIX_HIDE,

    // The character following the prefix character is underlined.
    PREFIX_SHOW
  };

  TextButton(ButtonListener* listener, const std::wstring& text);
  virtual ~TextButton();

  // Call SetText once per string in your set of possible values at button
  // creation time, so that it can contain the largest of them and avoid
  // resizing the button when the text changes.
  virtual void SetText(const std::wstring& text);
  std::wstring text() const { return UTF16ToWideHack(text_); }

  enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  void set_alignment(TextAlignment alignment) { alignment_ = alignment; }

  void set_prefix_type(PrefixType type) { prefix_type_ = type; }

  void set_icon_text_spacing(int icon_text_spacing) {
    icon_text_spacing_ = icon_text_spacing;
  }

  // Sets the icon.
  void SetIcon(const SkBitmap& icon);
  void SetHoverIcon(const SkBitmap& icon);
  void SetPushedIcon(const SkBitmap& icon);

  bool HasIcon() const { return !icon_.empty(); }

  // Meanings are reversed for right-to-left layouts.
  enum IconPlacement {
    ICON_ON_LEFT,
    ICON_ON_RIGHT
  };

  IconPlacement icon_placement() { return icon_placement_; }
  void set_icon_placement(IconPlacement icon_placement) {
    icon_placement_ = icon_placement;
  }

  // TextButton remembers the maximum display size of the text passed to
  // SetText. This method resets the cached maximum display size to the
  // current size.
  void ClearMaxTextSize();

  void set_max_width(int max_width) { max_width_ = max_width; }
  void SetFont(const gfx::Font& font);
  // Return the font used by this button.
  gfx::Font font() const { return font_; }

  void SetEnabledColor(SkColor color);
  void SetDisabledColor(SkColor color);
  void SetHighlightColor(SkColor color);
  void SetHoverColor(SkColor color);
  void SetTextHaloColor(SkColor color);
  void SetNormalHasBorder(bool normal_has_border);
  // Sets whether or not to show the hot and pushed states for the button icon
  // (if present) in addition to the normal state.  Defaults to true.
  void SetShowMultipleIconStates(bool show_multiple_icon_states);

  // Paint the button into the specified canvas. If |for_drag| is true, the
  // function paints a drag image representation into the canvas.
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual gfx::Size GetMinimumSize();
  virtual void SetEnabled(bool enabled);

  // Text colors.
  static const SkColor kEnabledColor;
  static const SkColor kHighlightColor;
  static const SkColor kDisabledColor;
  static const SkColor kHoverColor;

  // Returns views/TextButton.
  virtual std::string GetClassName() const;

 protected:
  SkBitmap icon() const { return icon_; }

  virtual void OnPaint(gfx::Canvas* canvas);

  // Called when enabled or disabled state changes, or the colors for those
  // states change.
  virtual void UpdateColor();

 private:
  // Updates text_size_ and max_text_size_ from the current text/font. This is
  // invoked when the font or text changes.
  void UpdateTextSize();

  // The text string that is displayed in the button.
  string16 text_;

  // The size of the text string.
  gfx::Size text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  gfx::Size max_text_size_;

  // The alignment of the text string within the button.
  TextAlignment alignment_;

  // The position of the icon.
  IconPlacement icon_placement_;

  // The font used to paint the text.
  gfx::Font font_;

  // Text color.
  SkColor color_;

  // State colors.
  SkColor color_enabled_;
  SkColor color_disabled_;
  SkColor color_highlight_;
  SkColor color_hover_;

  // An optional halo around text.
  SkColor text_halo_color_;
  bool has_text_halo_;

  // An icon displayed with the text.
  SkBitmap icon_;

  // An optional different version of the icon for hover state.
  SkBitmap icon_hover_;
  bool has_hover_icon_;

  // An optional different version of the icon for pushed state.
  SkBitmap icon_pushed_;
  bool has_pushed_icon_;

  // The width of the button will never be larger than this value. A value <= 0
  // indicates the width is not constrained.
  int max_width_;

  // This is true if normal state has a border frame; default is false.
  bool normal_has_border_;

  // Whether or not to show the hot and pushed icon states.
  bool show_multiple_icon_states_;

  PrefixType prefix_type_;

  // Space between icon and text.
  int icon_text_spacing_;

  DISALLOW_COPY_AND_ASSIGN(TextButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
