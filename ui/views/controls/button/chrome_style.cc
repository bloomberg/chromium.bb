// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/chrome_style.h"

#include <algorithm>

#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/painter.h"
#include "ui/views/background.h"

namespace views {
namespace {
const int kMinWidth = 72;
const int kMinHeight = 27;

// Fractional position between top and bottom of button where the
// gradient starts.
const SkScalar kGradientStartLocation = SkFloatToScalar(0.38f);
const SkColor kNormalBackgroundTopColor = SkColorSetRGB(0xf0, 0xf0, 0xf0);
const SkColor kNormalBackgroundBottomColor = SkColorSetRGB(0xe0, 0xe0, 0xe0);
const SkColor kHotBackgroundTopColor = SkColorSetRGB(0xf4, 0xf4, 0xf4);
const SkColor kHotBackgroundBottomColor = SkColorSetRGB(0xe4, 0xe4, 0xe4);
const SkColor kPushedBackgroundTopColor = SkColorSetRGB(0xeb, 0xeb, 0xeb);
const SkColor kPushedBackgroundBottomColor = SkColorSetRGB(0xdb, 0xdb, 0xdb);
const SkColor kDisabledBackgroundTopColor = SkColorSetRGB(0xed, 0xed, 0xed);
const SkColor kDisabledBackgroundBottomColor = SkColorSetRGB(0xde, 0xde, 0xde);

const SkScalar kShadowOffsetX = SkFloatToScalar(0.0f);
const SkScalar kShadowOffsetY = SkFloatToScalar(2.0f);
const SkColor kShadowColor = SkColorSetRGB(0x00, 0x00, 0x00);
const SkAlpha kNormalShadowAlpha = 0x13;
const SkAlpha kHotShadowAlpha = 0x1e;
const SkAlpha kPushedShadowAlpha = 0x00;
const SkAlpha kDisabledShadowAlpha = 0x00;

// The spec'ed radius in CSS is 2, but the Skia blur is much more diffuse than
// the CSS blur.  A radius of 1 produces a similar visual result.
const SkScalar kInsetShadowBlurRadius = SkFloatToScalar(1.0f);
const SkScalar kInsetShadowOffsetX = SkFloatToScalar(0.0f);
const SkScalar kInsetShadowOffsetY = SkFloatToScalar(1.0f);
const SkColor kInsetShadowColor = SkColorSetRGB(0xff, 0xff, 0xff);
const SkAlpha kNormalInsetShadowAlpha = 0xb7;
const SkAlpha kHotInsetShadowAlpha = 0xf2;
const SkAlpha kPushedInsetShadowAlpha = 0x23;
const SkAlpha kDisabledInsetShadowAlpha = 0x00;

const SkColor kEnabledTextColor = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kDisabledTextColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kHoverTextColor = SkColorSetRGB(0x0, 0x0, 0x0);

const SkColor kTextShadowColor = SkColorSetRGB(0xf0, 0xf0, 0xf0);
const int kTextShadowOffsetX = 0;
const int kTextShadowOffsetY = 1;

const int kBorderWidth = 1;
const int kBorderRadius = 2;
const SkColor kBorderNormalColor = SkColorSetARGB(0x3f, 0x0, 0x0, 0x0);
const SkColor kBorderActiveColor = SkColorSetARGB(0x4b, 0x0, 0x0, 0x0);
const SkColor kBorderDisabledColor = SkColorSetARGB(0x1d, 0x0, 0x0, 0x0);

const int kFocusRingWidth = 1;
const int kFocusRingRadius = 2;
const SkColor kFocusRingColor = SkColorSetRGB(0x4d, 0x90, 0xfe);

// Returns the uniform inset of the button from its local bounds.
int GetButtonInset() {
  return std::max(kBorderWidth, kFocusRingWidth);
}

class ChromeStyleTextButtonBackgroundPainter : public Painter {
 public:
  ChromeStyleTextButtonBackgroundPainter()
      : gradient_painter_(NULL),
        shadow_alpha_(kNormalShadowAlpha),
        inset_shadow_alpha_(kNormalInsetShadowAlpha) {
  }

  void SetColors(SkColor top, SkColor bottom) {
    static const int count = 3;
    SkColor colors[count] = { top, top, bottom };
    SkScalar pos[count] = {
      SkFloatToScalar(0.0f), kGradientStartLocation, SkFloatToScalar(1.0f) };

    gradient_painter_.reset(
        Painter::CreateVerticalMultiColorGradient(colors, pos, count));
  }

  void set_shadow_alpha(SkAlpha alpha) {
    shadow_alpha_ = alpha;
  }

  void set_inset_shadow_alpha(SkAlpha alpha) {
    inset_shadow_alpha_ = alpha;
  }

 private:
  // Overridden from Painter:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    PaintShadow(canvas, size);
    PaintGradientBackground(canvas, size);
    PaintInsetShadow(canvas, size);
  }

  void PaintGradientBackground(gfx::Canvas* canvas, const gfx::Size& size) {
    if (gradient_painter_.get())
      gradient_painter_->Paint(canvas, size);
  }

  void PaintShadow(gfx::Canvas* canvas, const gfx::Size& size) {
    SkPaint paint;

    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetA(kShadowColor, shadow_alpha_));

    SkRect shadow_rect = SkRect::MakeWH(size.width(), size.height());
    shadow_rect.offset(kShadowOffsetX, kShadowOffsetY);
    canvas->sk_canvas()->drawRect(shadow_rect, paint);
  }

  void PaintInsetShadow(gfx::Canvas* canvas, const gfx::Size& size) {
    if (inset_shadow_alpha_ == 0)
      return;

    canvas->sk_canvas()->saveLayerAlpha(NULL, inset_shadow_alpha_);

    // Outset shadow to ring of pixels outside the button, then offset.
    SkRect shadow_rect = SkRect::MakeWH(size.width(), size.height());
    float shadow_ring_width = SkScalarToFloat(kInsetShadowBlurRadius) * 2.0f;
    shadow_rect.outset(SkFloatToScalar(shadow_ring_width / 2.0f),
                       SkFloatToScalar(shadow_ring_width / 2.0f));
    shadow_rect.offset(kInsetShadowOffsetX, kInsetShadowOffsetY);

    // Clip to button region.
    SkRect clip_rect = SkRect::MakeWH(size.width(), size.height());

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkFloatToScalar(shadow_ring_width));
    paint.setColor(kInsetShadowColor);
    paint.setMaskFilter(
        SkBlurMaskFilter::Create(
            SkFloatToScalar(kInsetShadowBlurRadius),
            SkBlurMaskFilter::kNormal_BlurStyle));
    canvas->sk_canvas()->clipRect(clip_rect);
    canvas->sk_canvas()->drawRect(shadow_rect, paint);
    canvas->sk_canvas()->restore();
  }

  scoped_ptr<Painter> gradient_painter_;
  SkAlpha shadow_alpha_;
  SkAlpha inset_shadow_alpha_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStyleTextButtonBackgroundPainter);
};

class ChromeStyleTextButtonBackground : public Background {
 public:
  ChromeStyleTextButtonBackground()
      : painter_(new ChromeStyleTextButtonBackgroundPainter) {
  }

  // Overriden from Background
  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE {
    gfx::Rect bounds = view->GetLocalBounds();
    // Inset to the actual button region.
    int inset = GetButtonInset();
    bounds.Inset(inset, inset, inset, inset);
    Painter::PaintPainterAt(canvas, painter_.get(), bounds);
  }

  void SetColors(SkColor top, SkColor bottom) {
    painter_->SetColors(top, bottom);
    SetNativeControlColor(color_utils::AlphaBlend(top, bottom, 128));
  }

  void SetShadowAlpha(SkAlpha alpha) {
    painter_->set_shadow_alpha(alpha);
  }

  void SetInsetShadowAlpha(SkAlpha alpha) {
    painter_->set_inset_shadow_alpha(alpha);
  }

 private:
  scoped_ptr<ChromeStyleTextButtonBackgroundPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStyleTextButtonBackground);
};

class ChromeStyleTextButtonBorderPainter : public views::Painter {
 public:
  ChromeStyleTextButtonBorderPainter()
      // This value should be updated prior to rendering; it's set to a
      // well-defined value here defensively.
      : color_(SkColorSetRGB(0x0, 0x0, 0x0)) {
  }

  // Overriden from Painter
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(color_);
    paint.setStrokeWidth(kBorderWidth);

    // Inset by 1/2 pixel to align the stroke with pixel centers.
    SkScalar inset = SkFloatToScalar(0.5f);
    SkRect rect = SkRect::MakeLTRB(inset, inset,
                                   SkIntToScalar(size.width()) - inset,
                                   SkIntToScalar(size.height()) - inset);

    canvas->sk_canvas()->drawRoundRect(
        rect, kBorderRadius, kBorderRadius, paint);
  }

  void set_color(SkColor color) {
    color_ = color;
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStyleTextButtonBorderPainter);
};


class ChromeStyleTextButtonBorder : public views::Border {
 public:
  ChromeStyleTextButtonBorder()
      : painter_(new ChromeStyleTextButtonBorderPainter) {
  }

  // Overriden from Border
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE {
    gfx::Rect bounds = view.GetLocalBounds();
    int border_inset = GetButtonInset() - kBorderWidth;
    bounds.Inset(border_inset, border_inset, border_inset, border_inset);
    Painter::PaintPainterAt(canvas, painter_.get(), bounds);
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    const int inset = GetButtonInset();
    return gfx::Insets(inset, inset, inset, inset);
  }

  void SetColor(SkColor color) {
    painter_->set_color(color);
  }

 private:
  scoped_ptr<ChromeStyleTextButtonBorderPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStyleTextButtonBorder);
};

class ChromeStyleFocusBorder : public views::FocusBorder {
 public:
  ChromeStyleFocusBorder() {}

  // Overriden from views::FocusBorder
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(kFocusRingWidth));
    paint.setColor(kFocusRingColor);

    SkScalar inset = SkFloatToScalar(GetButtonInset() - kFocusRingWidth / 2.0f);
    gfx::Rect view_bounds(view.GetLocalBounds());
    SkRect rect = SkRect::MakeLTRB(
        SkIntToScalar(view_bounds.x()) + inset,
        SkIntToScalar(view_bounds.y()) + inset,
        SkIntToScalar(view_bounds.width()) - inset,
        SkIntToScalar(view_bounds.height()) - inset);
    canvas->sk_canvas()->drawRoundRect(rect, SkIntToScalar(kFocusRingRadius),
        SkIntToScalar(kFocusRingRadius), paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeStyleFocusBorder);
};

class ChromeStyleStateChangedUpdater
    : public CustomButtonStateChangedDelegate {
 public:
  // The background and border may be NULL.
  ChromeStyleStateChangedUpdater(TextButton* button,
                                 ChromeStyleTextButtonBackground* background,
                                 ChromeStyleTextButtonBorder* border)
      : button_(button),
        background_(background),
        border_(border),
        prior_state_(button->state()) {
    SetBackgroundForState(button->state());
    SetShadowForState(button->state());
    SetBorderColorForState(button->state());
  }

  virtual void StateChanged(CustomButton::ButtonState state) OVERRIDE {
    SetBackgroundForState(state);

    // Update text shadow when transitioning to/from pushed state.
    if (state == CustomButton::STATE_PRESSED ||
        prior_state_ == CustomButton::STATE_PRESSED) {
      SetShadowForState(state);
    }

    // Update border color.  We need to change it in all cases except hot
    // followed by pushed.
    if (!(state == CustomButton::STATE_PRESSED &&
          prior_state_ == CustomButton::STATE_HOVERED)) {
      SetBorderColorForState(state);
    }

    prior_state_ = state;
  }

 private:
  void SetBackgroundForState(CustomButton::ButtonState state) {
    if (!background_)
      return;

    SkColor top;
    SkColor bottom;
    SkAlpha shadow_alpha;
    SkAlpha inset_shadow_alpha;

    switch (state) {
      case CustomButton::STATE_NORMAL:
        top = kNormalBackgroundTopColor;
        bottom = kNormalBackgroundBottomColor;
        shadow_alpha = kNormalShadowAlpha;
        inset_shadow_alpha = kNormalInsetShadowAlpha;
        break;

      case CustomButton::STATE_HOVERED:
        top = kHotBackgroundTopColor;
        bottom = kHotBackgroundBottomColor;
        shadow_alpha = kHotShadowAlpha;
        inset_shadow_alpha = kHotInsetShadowAlpha;
        break;

      case CustomButton::STATE_PRESSED:
        top = kPushedBackgroundTopColor;
        bottom = kPushedBackgroundBottomColor;
        shadow_alpha = kPushedShadowAlpha;
        inset_shadow_alpha = kPushedInsetShadowAlpha;
        break;

      case CustomButton::STATE_DISABLED:
        top = kDisabledBackgroundTopColor;
        bottom = kDisabledBackgroundBottomColor;
        shadow_alpha = kDisabledShadowAlpha;
        inset_shadow_alpha = kDisabledInsetShadowAlpha;
        break;

      default:
        NOTREACHED();
        return;
        break;
    }

    background_->SetColors(top, bottom);
    background_->SetShadowAlpha(shadow_alpha);
    background_->SetInsetShadowAlpha(inset_shadow_alpha);
  }

  void SetShadowForState(CustomButton::ButtonState state) {
    if (state == CustomButton::STATE_PRESSED) {
      // Turn off text shadow.
      button_->ClearEmbellishing();
    } else {
      button_->SetTextShadowColors(kTextShadowColor, kTextShadowColor);
      button_->SetTextShadowOffset(kTextShadowOffsetX, kTextShadowOffsetY);
    }
  }

  void SetBorderColorForState(CustomButton::ButtonState state) {
    if (!border_)
      return;

    SkColor border_color;

    switch (state) {
      case CustomButton::STATE_NORMAL:
        border_color = kBorderNormalColor;
        break;

      case CustomButton::STATE_HOVERED:
      case CustomButton::STATE_PRESSED:
        border_color = kBorderActiveColor;
        break;

      case CustomButton::STATE_DISABLED:
        border_color = kBorderDisabledColor;
        break;

      default:
        NOTREACHED();
        return;
        break;
    }

    border_->SetColor(border_color);
  }

  // Weak pointer to the associated button.
  TextButton* button_;

  // Weak pointers to background and border owned by the CustomButton.
  ChromeStyleTextButtonBackground* background_;
  ChromeStyleTextButtonBorder* border_;

  CustomButton::ButtonState prior_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStyleStateChangedUpdater);
};
}  // namespace


void ApplyChromeStyle(TextButton* button) {
  button->set_focusable(true);
  button->set_request_focus_on_press(false);

  button->set_alignment(TextButton::ALIGN_CENTER);
  button->set_min_width(kMinWidth);
  button->set_min_height(kMinHeight);

  if (button->is_default())
    button->SetFont(button->font().DeriveFont(0, gfx::Font::BOLD));

  button->SetEnabledColor(kEnabledTextColor);
  button->SetDisabledColor(kDisabledTextColor);
  button->SetHoverColor(kHoverTextColor);

  ChromeStyleTextButtonBackground* background =
      new ChromeStyleTextButtonBackground;
  button->set_background(background);

  ChromeStyleTextButtonBorder* border = new ChromeStyleTextButtonBorder;
  button->set_border(border);

  button->set_focus_border(new ChromeStyleFocusBorder);

  ChromeStyleStateChangedUpdater* state_changed_updater =
      new ChromeStyleStateChangedUpdater(button, background, border);

  button->set_state_changed_delegate(state_changed_updater);
}

}  // namespace views
