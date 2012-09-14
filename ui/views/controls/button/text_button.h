// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBorder
//
//  A Border subclass that paints a TextButton's background layer -
//  basically the button frame in the hot/pushed states.
//
// Note that this type of button is not focusable by default and will not be
// part of the focus chain.  Call set_focusable(true) to make it part of the
// focus chain.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT TextButtonBorder : public Border {
 public:
  TextButtonBorder();
  virtual ~TextButtonBorder();

  // By default BS_NORMAL is drawn with no border.  Call this to instead draw it
  // with the same border as the "hot" state.
  // TODO(pkasting): You should also call set_animate_on_state_change(false) on
  // the button in this case... we should fix this.
  void copy_normal_set_to_hot_set() { set_normal_set(hot_set_); }

 protected:
  struct BorderImageSet {
    const gfx::ImageSkia* top_left;
    const gfx::ImageSkia* top;
    const gfx::ImageSkia* top_right;
    const gfx::ImageSkia* left;
    const gfx::ImageSkia* center;
    const gfx::ImageSkia* right;
    const gfx::ImageSkia* bottom_left;
    const gfx::ImageSkia* bottom;
    const gfx::ImageSkia* bottom_right;
  };

  void Paint(const View& view,
             gfx::Canvas* canvas,
             const BorderImageSet& set) const;

  void set_normal_set(const BorderImageSet& set) { normal_set_ = set; }
  void set_hot_set(const BorderImageSet& set) { hot_set_ = set; }
  void set_pushed_set(const BorderImageSet& set) { pushed_set_ = set; }
  void set_vertical_padding(int vertical_padding) {
    vertical_padding_ = vertical_padding;
  }

 private:
  // Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE;
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;

  BorderImageSet normal_set_;
  BorderImageSet hot_set_;
  BorderImageSet pushed_set_;

  int vertical_padding_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonBorder);
};


////////////////////////////////////////////////////////////////////////////////
//
// TextButtonNativeThemeBorder
//
//  A Border subclass that paints a TextButton's background layer using the
//  platform's native theme look.  This handles normal/disabled/hot/pressed
//  states, with possible animation between states.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT TextButtonNativeThemeBorder : public Border {
 public:
  explicit TextButtonNativeThemeBorder(NativeThemeDelegate* delegate);
  virtual ~TextButtonNativeThemeBorder();

  // Implementation of Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE;
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;

 private:
  // The delegate the controls the appearance of this border.
  NativeThemeDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonNativeThemeBorder);
};


////////////////////////////////////////////////////////////////////////////////
//
// TextButtonBase
//
//  A base class for different types of buttons, like push buttons, radio
//  buttons, and checkboxes, that do not depend on native components for
//  look and feel. TextButton reserves space for the largest string
//  passed to SetText. To reset the cached max size invoke ClearMaxTextSize.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT TextButtonBase : public CustomButton,
                                    public NativeThemeDelegate {
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

  virtual ~TextButtonBase();

  // Call SetText once per string in your set of possible values at button
  // creation time, so that it can contain the largest of them and avoid
  // resizing the button when the text changes.
  virtual void SetText(const string16& text);
  const string16& text() const { return text_; }

  enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
  };

  void set_alignment(TextAlignment alignment) { alignment_ = alignment; }

  void set_prefix_type(PrefixType type) { prefix_type_ = type; }

  const ui::Animation* GetAnimation() const;

  void SetIsDefault(bool is_default);
  bool is_default() const { return is_default_; }

  // Set whether the button text can wrap on multiple lines.
  // Default is false.
  void SetMultiLine(bool multi_line);

  // Return whether the button text can wrap on multiple lines.
  bool multi_line() const { return multi_line_; }

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
  // The shadow color used is determined by whether the widget is active or
  // inactive. Both possible colors are set in this method, and the
  // appropriate one is chosen during Paint.
  void SetTextShadowColors(SkColor active_color, SkColor inactive_color);
  void SetTextShadowOffset(int x, int y);

  // Sets whether or not to show the hot and pushed states for the button icon
  // (if present) in addition to the normal state.  Defaults to true.
  bool show_multiple_icon_states() const { return show_multiple_icon_states_; }
  void SetShowMultipleIconStates(bool show_multiple_icon_states);

  // Clears halo and shadow settings.
  void ClearEmbellishing();

  // Paint the button into the specified canvas. If |mode| is |PB_FOR_DRAG|, the
  // function paints a drag image representation into the canvas.
  enum PaintButtonMode { PB_NORMAL, PB_FOR_DRAG };
  virtual void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual int GetHeightForWidth(int w) OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 protected:
  TextButtonBase(ButtonListener* listener, const string16& text);

  // Called when enabled or disabled state changes, or the colors for those
  // states change.
  virtual void UpdateColor();

  // Updates text_size_ and max_text_size_ from the current text/font. This is
  // invoked when the font or text changes.
  void UpdateTextSize();

  // Calculate the size of the text size without setting any of the members.
  void CalculateTextSize(gfx::Size* text_size, int max_width);

  // Overridden from NativeThemeDelegate:
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;
  virtual ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual const ui::Animation* GetThemeAnimation() const OVERRIDE;
  virtual ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;

  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  virtual gfx::Rect GetTextBounds() const;

  int ComputeCanvasStringFlags() const;

  // Calculate the bounds of the content of this button, including any extra
  // width needed on top of the text width.
  gfx::Rect GetContentBounds(int extra_width) const;

  // The text string that is displayed in the button.
  string16 text_;

  // The size of the text string.
  gfx::Size text_size_;

  // Track the size of the largest text string seen so far, so that
  // changing text_ will not resize the button boundary.
  gfx::Size max_text_size_;

  // The alignment of the text string within the button.
  TextAlignment alignment_;

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

  // Optional shadow text colors for active and inactive widget states.
  SkColor active_text_shadow_color_;
  SkColor inactive_text_shadow_color_;
  bool has_shadow_;
  // Space between text and shadow. Defaults to (1,1).
  gfx::Point shadow_offset_;

  // The width of the button will never be larger than this value. A value <= 0
  // indicates the width is not constrained.
  int max_width_;

  // Whether or not to show the hot and pushed icon states.
  bool show_multiple_icon_states_;

  // Whether or not the button appears and behaves as the default button in its
  // current context.
  bool is_default_;

  // Whether the text button should handle its text string as multi-line.
  bool multi_line_;

  PrefixType prefix_type_;

  DISALLOW_COPY_AND_ASSIGN(TextButtonBase);
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
class VIEWS_EXPORT TextButton : public TextButtonBase {
 public:
  // The button's class name.
  static const char kViewClassName[];

  TextButton(ButtonListener* listener, const string16& text);
  virtual ~TextButton();

  void set_icon_text_spacing(int icon_text_spacing) {
    icon_text_spacing_ = icon_text_spacing;
  }

  // Sets the icon.
  virtual void SetIcon(const gfx::ImageSkia& icon);
  virtual void SetHoverIcon(const gfx::ImageSkia& icon);
  virtual void SetPushedIcon(const gfx::ImageSkia& icon);

  bool HasIcon() const { return !icon_.isNull(); }

  // Meanings are reversed for right-to-left layouts.
  enum IconPlacement {
    ICON_ON_LEFT,
    ICON_ON_RIGHT,
    ICON_CENTERED  // Centered is valid only when text is empty.
  };

  IconPlacement icon_placement() { return icon_placement_; }
  void set_icon_placement(IconPlacement icon_placement) {
    // ICON_CENTERED works only when |text_| is empty.
    DCHECK((icon_placement != ICON_CENTERED) || text_.empty());
    icon_placement_ = icon_placement;
  }

  void set_ignore_minimum_size(bool ignore_minimum_size);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from TextButtonBase:
  virtual void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) OVERRIDE;

 protected:
  gfx::ImageSkia icon() const { return icon_; }

  virtual const gfx::ImageSkia& GetImageToPaint() const;

  // Overridden from NativeThemeDelegate:
  virtual ui::NativeTheme::Part GetThemePart() const OVERRIDE;

  // Overridden from TextButtonBase:
  virtual void GetExtraParams(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual gfx::Rect GetTextBounds() const OVERRIDE;

 private:
  // The position of the icon.
  IconPlacement icon_placement_;

  // An icon displayed with the text.
  gfx::ImageSkia icon_;

  // An optional different version of the icon for hover state.
  gfx::ImageSkia icon_hover_;
  bool has_hover_icon_;

  // An optional different version of the icon for pushed state.
  gfx::ImageSkia icon_pushed_;
  bool has_pushed_icon_;

  // Space between icon and text.
  int icon_text_spacing_;

  // True if the button should ignore the minimum size for the platform. Default
  // is true. Set to false to prevent narrower buttons.
  bool ignore_minimum_size_;

  DISALLOW_COPY_AND_ASSIGN(TextButton);
};

////////////////////////////////////////////////////////////////////////////////
//
// NativeTextButton
//
//  A TextButton that uses the NativeTheme border and sets some properties,
//  like ignore-minimize-size and text alignment minimum size.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT NativeTextButton : public TextButton {
 public:
  // The button's class name.
  static const char kViewClassName[];

  explicit NativeTextButton(ButtonListener* listener);
  NativeTextButton(ButtonListener* listener, const string16& text);

  // Overridden from View:
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from TextButton:
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 private:
  void Init();

  // Overridden from TextButton:
  virtual void GetExtraParams(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NativeTextButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_TEXT_BUTTON_H_
