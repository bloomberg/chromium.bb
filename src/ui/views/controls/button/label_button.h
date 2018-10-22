// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include <array>
#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_theme_delegate.h"
#include "ui/views/style/typography.h"

namespace views {

class InkDropContainerView;
class LabelButtonBorder;
class LabelButtonLabel;

// LabelButton is a button with text and an icon, it's not focusable by default.
class VIEWS_EXPORT LabelButton : public Button, public NativeThemeDelegate {
 public:
  // The length of the hover fade animation.
  static const int kHoverAnimationDurationMs;

  static const char kViewClassName[];

  // Creates a LabelButton with ButtonPressed() events sent to |listener| and
  // label |text|. |button_context| is a value from views::style::TextContext
  // and determines the appearance of |text|.
  LabelButton(ButtonListener* listener,
              const base::string16& text,
              int button_context = style::CONTEXT_BUTTON);
  ~LabelButton() override;

  // Gets or sets the image shown for the specified button state.
  // GetImage returns the image for STATE_NORMAL if the state's image is empty.
  virtual gfx::ImageSkia GetImage(ButtonState for_state) const;
  void SetImage(ButtonState for_state, const gfx::ImageSkia& image);

  // Gets or sets the text shown on the button.
  const base::string16& GetText() const;
  virtual void SetText(const base::string16& text);

  // Sets the text color shown for the specified button |for_state| to |color|.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Sets the text colors shown for the non-disabled states to |color|.
  virtual void SetEnabledTextColors(SkColor color);

  // Sets drop shadows underneath the text.
  void SetTextShadows(const gfx::ShadowValues& shadows);

  // Sets whether subpixel rendering is used on the label.
  void SetTextSubpixelRenderingEnabled(bool enabled);

  // Sets the elide behavior of this button.
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Sets the horizontal alignment used for the button; reversed in RTL. The
  // optional image will lead the text, unless the button is right-aligned.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  void SetMinSize(const gfx::Size& min_size);
  void SetMaxSize(const gfx::Size& max_size);

  // Gets or sets the option to handle the return key; false by default.
  bool is_default() const { return is_default_; }
  void SetIsDefault(bool is_default);

  // Gets or sets the button's overall style; the default is |STYLE_TEXTBUTTON|.
  // DEPRECATED: ButtonStyle is deprecated. Use MdTextButton in place of
  // |STYLE_BUTTON|.
  ButtonStyle style() const { return style_; }
  void SetStyleDeprecated(ButtonStyle style);

  // Sets the spacing between the image and the text.
  void SetImageLabelSpacing(int spacing);

  // Creates the default border for this button. This can be overridden by
  // subclasses.
  virtual std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const;

  // View:
  void SetBorder(std::unique_ptr<Border> border) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;
  const char* GetClassName() const override;
  void EnableCanvasFlippingForRTLUI(bool flip) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const override;

 protected:
  ImageView* image() const { return image_; }
  Label* label() const;
  InkDropContainerView* ink_drop_container() const {
    return ink_drop_container_;
  }

  bool explicitly_set_normal_color() const {
    return explicitly_set_colors_[STATE_NORMAL];
  }

  // Returns the available area for the label and image. Subclasses can change
  // these bounds if they need room to do manual painting.
  virtual gfx::Rect GetChildAreaBounds();

  // Returns true if the CreateInkDrop*() methods should create flood fill ink
  // drop components.
  virtual bool ShouldUseFloodFillInkDrop() const;

  // View:
  void OnFocus() override;
  void OnBlur() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Button:
  void StateChanged(ButtonState old_state) override;

  // Fills |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  // Resets colors from the NativeTheme, explicitly set colors are unchanged.
  virtual void ResetColorsFromNativeTheme();

  // Changes the visual styling of this button to reflect the state of
  // |is_default()|.
  virtual void UpdateStyleToIndicateDefaultStatus();

  // Updates the image view to contain the appropriate button state image.
  void UpdateImage();

  // Updates the border as per the NativeTheme, unless a different border was
  // set with SetBorder.
  void UpdateThemedBorder();

  // NativeThemeDelegate:
  gfx::Rect GetThemePaintRect() const override;

  const std::array<bool, STATE_COUNT>& explicitly_set_colors() const {
    return explicitly_set_colors_;
  }
  void set_explicitly_set_colors(const std::array<bool, STATE_COUNT>& colors) {
    explicitly_set_colors_ = colors;
  }

 private:
  void SetTextInternal(const base::string16& text);

  // View:
  void ChildPreferredSizeChanged(View* child) override;

  // NativeThemeDelegate:
  ui::NativeTheme::Part GetThemePart() const override;
  ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  const gfx::Animation* GetThemeAnimation() const override;
  ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;

  // Resets |cached_preferred_size_| and marks |cached_preferred_size_valid_|
  // as false.
  void ResetCachedPreferredSize();

  // Gets the preferred size (without respecting min_size_ or max_size_), but
  // does not account for the label. This is shared between GetHeightForWidth
  // and CalculatePreferredSize. GetHeightForWidth will subtract the width
  // returned from this method to get width available for the label while
  // CalculatePreferredSize will just add the preferred width from the label.
  // Both methods will then use the max of inset height + label height and this
  // height as total height, and clamp to min/max sizes as appropriate.
  gfx::Size GetUnclampedSizeWithoutLabel() const;

  // Updates additional state related to focus or default status, rather than
  // merely the Button::state(). E.g. ensures the label text color is
  // correct for the current background.
  void ResetLabelEnabledColor();

  // The image and label shown in the button.
  ImageView* image_;
  LabelButtonLabel* label_;

  // A separate view is necessary to hold the ink drop layer so that it can
  // be stacked below |image_| and on top of |label_|, without resorting to
  // drawing |label_| on a layer (which can mess with subpixel anti-aliasing).
  InkDropContainerView* ink_drop_container_;

  // The cached font lists in the normal and default button style. The latter
  // may be bold.
  gfx::FontList cached_normal_font_list_;
  gfx::FontList cached_default_button_font_list_;

  // The images and colors for each button state.
  gfx::ImageSkia button_state_images_[STATE_COUNT];
  SkColor button_state_colors_[STATE_COUNT];

  // Used to track whether SetTextColor() has been invoked.
  std::array<bool, STATE_COUNT> explicitly_set_colors_;

  // |min_size_| and |max_size_| may be set to clamp the preferred size.
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Cache the last computed preferred size.
  mutable gfx::Size cached_preferred_size_;
  mutable bool cached_preferred_size_valid_;

  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_;

  // The button's overall style.
  ButtonStyle style_;

  // True if current border was set by UpdateThemedBorder. Defaults to true.
  bool border_is_themed_border_;

  // Spacing between the image and the text.
  int image_label_spacing_;

  // Alignment of the button. This can be different from the alignment of the
  // text; for example, the label may be set to ALIGN_TO_HEAD (alignment matches
  // text direction) while |this| is laid out as ALIGN_LEFT (alignment matches
  // UI direction).
  gfx::HorizontalAlignment horizontal_alignment_;

  DISALLOW_COPY_AND_ASSIGN(LabelButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
