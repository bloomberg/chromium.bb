// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include "app/resource_bundle.h"
#include "gfx/canvas_skia.h"
#include "gfx/favicon_size.h"
#include "grit/app_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/text_button.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

// Background color when the menu item is selected.
#if defined(OS_CHROMEOS)
static const SkColor kSelectedBackgroundColor = SkColorSetRGB(0xDC, 0xE4, 0xFA);
#else
static const SkColor kSelectedBackgroundColor = SkColorSetRGB(246, 249, 253);
#endif

// Size of the radio button inciator.
static const int kSelectedIndicatorSize = 5;
static const int kIndicatorSize = 10;

// Used for the radio indicator. See theme_draw for details.
static const double kGradientStop = .5;
static const SkColor kGradient0 = SkColorSetRGB(255, 255, 255);
static const SkColor kGradient1 = SkColorSetRGB(255, 255, 255);
static const SkColor kGradient2 = SkColorSetRGB(0xD8, 0xD8, 0xD8);
static const SkColor kBaseStroke = SkColorSetRGB(0x8F, 0x8F, 0x8F);
static const SkColor kRadioButtonIndicatorGradient0 =
    SkColorSetRGB(0, 0, 0);
static const SkColor kRadioButtonIndicatorGradient1 =
    SkColorSetRGB(0x83, 0x83, 0x83);

static const SkColor kIndicatorStroke = SkColorSetRGB(0, 0, 0);

gfx::Size MenuItemView::GetPreferredSize() {
  const gfx::Font& font = MenuConfig::instance().font;
  // TODO(sky): this is a workaround until I figure out why font.height()
  // isn't returning the right thing. We really only want to include
  // kFavIconSize if we're showing icons.
  int content_height = std::max(kFavIconSize, font.height());
  return gfx::Size(
      font.GetStringWidth(title_) + label_start_ + item_right_margin_ +
          GetChildPreferredWidth(),
      content_height + GetBottomMargin() + GetTopMargin());
}

void MenuItemView::Paint(gfx::Canvas* canvas, bool for_drag) {
  const MenuConfig& config = MenuConfig::instance();
  bool render_selection =
      (!for_drag && IsSelected() &&
       parent_menu_item_->GetSubmenu()->GetShowSelection(this) &&
       GetChildViewCount() == 0);

  int icon_x = config.item_left_margin;
  int top_margin = GetTopMargin();
  int bottom_margin = GetBottomMargin();
  int icon_y = top_margin + (height() - config.item_top_margin -
                             bottom_margin - config.check_height) / 2;
  int icon_height = config.check_height;
  int available_height = height() - top_margin - bottom_margin;

  // Render the background. As MenuScrollViewContainer draws the background, we
  // only need the background when we want it to look different, as when we're
  // selected.
  if (render_selection)
    canvas->AsCanvasSkia()->drawColor(kSelectedBackgroundColor,
                                      SkXfermode::kSrc_Mode);

  // Render the check.
  if (type_ == CHECKBOX && GetDelegate()->IsItemChecked(GetCommand())) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap* check = rb.GetBitmapNamed(IDR_MENU_CHECK);
    // Don't use config.check_width here as it's padded to force more padding.
    gfx::Rect check_bounds(icon_x, icon_y, check->width(), icon_height);
    AdjustBoundsForRTLUI(&check_bounds);
    canvas->DrawBitmapInt(*check, check_bounds.x(), check_bounds.y());
  } else if (type_ == RADIO) {
    // This code comes from theme_draw.cc. See it for details.
    canvas->TranslateInt(
        icon_x,
        top_margin + (height() - top_margin - bottom_margin -
                      kIndicatorSize) / 2);

    SkPoint gradient_points[3];
    gradient_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
    gradient_points[1].set(
        SkIntToScalar(0),
        SkIntToScalar(static_cast<int>(kIndicatorSize * kGradientStop)));
    gradient_points[2].set(SkIntToScalar(0), SkIntToScalar(kIndicatorSize));
    SkColor gradient_colors[3] = { kGradient0, kGradient1, kGradient2 };
    SkShader* shader = SkGradientShader::CreateLinear(
        gradient_points, gradient_colors, NULL, arraysize(gradient_points),
        SkShader::kClamp_TileMode, NULL);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setShader(shader);
    shader->unref();
    int radius = kIndicatorSize / 2;
    canvas->AsCanvasSkia()->drawCircle(radius, radius, radius, paint);

    paint.setStrokeWidth(SkIntToScalar(0));
    paint.setShader(NULL);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kBaseStroke);
    canvas->AsCanvasSkia()->drawCircle(radius, radius, radius, paint);

    if (GetDelegate()->IsItemChecked(GetCommand())) {
      SkPoint selected_gradient_points[2];
      selected_gradient_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
      selected_gradient_points[1].set(
          SkIntToScalar(0),
          SkIntToScalar(kSelectedIndicatorSize));
      SkColor selected_gradient_colors[2] = { kRadioButtonIndicatorGradient0,
                                              kRadioButtonIndicatorGradient1 };
      shader = SkGradientShader::CreateLinear(
          selected_gradient_points, selected_gradient_colors, NULL,
          arraysize(selected_gradient_points), SkShader::kClamp_TileMode, NULL);
      paint.setShader(shader);
      shader->unref();
      paint.setStyle(SkPaint::kFill_Style);
      canvas->AsCanvasSkia()->drawCircle(radius, radius,
                                         kSelectedIndicatorSize / 2, paint);

      paint.setStrokeWidth(SkIntToScalar(0));
      paint.setShader(NULL);
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setColor(kIndicatorStroke);
      canvas->AsCanvasSkia()->drawCircle(radius, radius,
                                         kSelectedIndicatorSize / 2, paint);
    }

    canvas->TranslateInt(
        -icon_x,
        -(top_margin + (height() - top_margin - bottom_margin -
                        kIndicatorSize) / 2));
  }

  // Render the foreground.
#if defined(OS_CHROMEOS)
  SkColor fg_color =
      IsEnabled() ? SK_ColorBLACK : SkColorSetRGB(0x80, 0x80, 0x80);
#else
  SkColor fg_color =
      IsEnabled() ? TextButton::kEnabledColor : TextButton::kDisabledColor;
#endif
  const gfx::Font& font = MenuConfig::instance().font;
  int accel_width = parent_menu_item_->GetSubmenu()->max_accelerator_width();
  int width = this->width() - item_right_margin_ - label_start_ - accel_width;
  gfx::Rect text_bounds(label_start_, top_margin +
                        (available_height - font.height()) / 2, width,
                        font.height());
  text_bounds.set_x(MirroredLeftPointForRect(text_bounds));
  canvas->DrawStringInt(GetTitle(), font, fg_color,
                        text_bounds.x(), text_bounds.y(), text_bounds.width(),
                        text_bounds.height(),
                        GetRootMenuItem()->GetDrawStringFlags());

  PaintAccelerator(canvas);

  // Render the icon.
  if (icon_.width() > 0) {
    gfx::Rect icon_bounds(config.item_left_margin,
                          top_margin + (height() - top_margin -
                          bottom_margin - icon_.height()) / 2,
                          icon_.width(),
                          icon_.height());
    icon_bounds.set_x(MirroredLeftPointForRect(icon_bounds));
    canvas->DrawBitmapInt(icon_, icon_bounds.x(), icon_bounds.y());
  }

  // Render the submenu indicator (arrow).
  if (HasSubmenu()) {
    gfx::Rect arrow_bounds(this->width() - item_right_margin_ +
                           config.label_to_arrow_padding,
                           top_margin + (available_height -
                                         config.arrow_width) / 2,
                           config.arrow_width, height());
    AdjustBoundsForRTLUI(&arrow_bounds);
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_MENU_ARROW),
                          arrow_bounds.x(), arrow_bounds.y());
  }
}

}  // namespace views
