// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/tile_item_view.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kTopPadding = 5;
constexpr int kTileSize = 90;
constexpr int kIconTitleSpacing = 6;

constexpr int kIconSelectedSize = 56;
constexpr int kIconSelectedCornerRadius = 4;
// Icon selected color, #000 8%.
constexpr int kIconSelectedColor = SkColorSetARGBMacro(0x14, 0x00, 0x00, 0x00);

// The background image source for badge.
class BadgeBackgroundImageSource : public gfx::CanvasImageSource {
 public:
  explicit BadgeBackgroundImageSource(int size)
      : CanvasImageSource(gfx::Size(size, size), false),
        radius_(static_cast<float>(size / 2)) {}
  ~BadgeBackgroundImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(SK_ColorWHITE);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(radius_, radius_), radius_, flags);
  }

  const float radius_;

  DISALLOW_COPY_AND_ASSIGN(BadgeBackgroundImageSource);
};

}  // namespace

namespace app_list {

TileItemView::TileItemView()
    : views::Button(this),
      parent_background_color_(SK_ColorTRANSPARENT),
      icon_(new views::ImageView),
      badge_(nullptr),
      title_(new views::Label),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  if (features::IsAppListFocusEnabled())
    SetFocusBehavior(FocusBehavior::ALWAYS);
  // Prevent the icon view from interfering with our mouse events.
  icon_->set_can_process_events_within_subtree(false);
  icon_->SetVerticalAlignment(views::ImageView::LEADING);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);
  title_->SetFontList(rb.GetFontList(kItemTextFontStyle));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetHandlesTooltips(false);

  AddChildView(icon_);
  if (features::IsPlayStoreAppSearchEnabled()) {
    badge_ = new views::ImageView();
    badge_->set_can_process_events_within_subtree(false);
    badge_->SetVerticalAlignment(views::ImageView::LEADING);
    AddChildView(badge_);
  }
  AddChildView(title_);
}

TileItemView::~TileItemView() = default;

void TileItemView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  UpdateBackgroundColor();

  if (selected)
    NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
}

void TileItemView::SetParentBackgroundColor(SkColor color) {
  parent_background_color_ = color;
  UpdateBackgroundColor();
}

void TileItemView::SetHoverStyle(HoverStyle hover_style) {
  if (hover_style == HOVER_STYLE_DARKEN_BACKGROUND) {
    image_shadow_animator_.reset();
    return;
  }

  image_shadow_animator_.reset(new ImageShadowAnimator(this));
  image_shadow_animator_->animation()->SetTweenType(
      gfx::Tween::FAST_OUT_SLOW_IN);
  image_shadow_animator_->SetStartAndEndShadows(IconStartShadows(),
                                                IconEndShadows());
}

void TileItemView::SetIcon(const gfx::ImageSkia& icon) {
  if (image_shadow_animator_) {
    // Will call icon_->SetImage synchronously.
    image_shadow_animator_->SetOriginalImage(icon);
    return;
  }

  icon_->SetImage(icon);
}

void TileItemView::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  if (!badge_)
    return;

  if (badge_icon.isNull()) {
    badge_->SetVisible(false);
    return;
  }

  const int size = kBadgeBackgroundRadius * 2;
  gfx::ImageSkia background(std::make_unique<BadgeBackgroundImageSource>(size),
                            gfx::Size(size, size));
  gfx::ImageSkia icon_with_background =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(background, badge_icon);

  gfx::ShadowValues shadow_values;
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, 1), 0, SkColorSetARGB(0x33, 0, 0, 0)));
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, 1), 2, SkColorSetARGB(0x33, 0, 0, 0)));
  badge_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      icon_with_background, shadow_values));
  badge_->SetVisible(true);
}

void TileItemView::SetTitle(const base::string16& title) {
  title_->SetText(title);
  SetAccessibleName(title);
}

void TileItemView::StateChanged(ButtonState old_state) {
  UpdateBackgroundColor();
}

void TileItemView::PaintButtonContents(gfx::Canvas* canvas) {
  if (!is_fullscreen_app_list_enabled_ || !selected_)
    return;

  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  if (is_recommendation_) {
    rect.Inset((rect.width() - kGridSelectedSize) / 2,
               (rect.height() - kGridSelectedSize) / 2);
    flags.setColor(kGridSelectedColor);
    canvas->DrawRoundRect(gfx::RectF(rect), kGridSelectedCornerRadius, flags);
  } else {
    const int kLeftRightPadding = (rect.width() - kIconSelectedSize) / 2;
    rect.Inset(kLeftRightPadding, 0);
    rect.set_height(kIconSelectedSize);
    flags.setColor(kIconSelectedColor);
    canvas->DrawRoundRect(gfx::RectF(rect), kIconSelectedCornerRadius, flags);
  }
}

void TileItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  rect.Inset(0, kTopPadding, 0, 0);
  icon_->SetBoundsRect(rect);

  rect.Inset(0, kGridIconDimension + kIconTitleSpacing, 0, 0);
  rect.set_height(title_->GetPreferredSize().height());
  title_->SetBoundsRect(rect);
}

const char* TileItemView::GetClassName() const {
  return "TileItemView";
}

void TileItemView::OnFocus() {
  SetSelected(true);
}

void TileItemView::OnBlur() {
  SetSelected(false);
}

void TileItemView::ImageShadowAnimationProgressed(
    ImageShadowAnimator* animator) {
  icon_->SetImage(animator->shadow_image());
}

gfx::Size TileItemView::CalculatePreferredSize() const {
  return gfx::Size(kTileSize, kTileSize);
}

bool TileItemView::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  bool handled = title_->GetTooltipText(p, tooltip);
  title_->SetHandlesTooltips(false);
  return handled;
}

void TileItemView::UpdateBackgroundColor() {
  SkColor background_color = parent_background_color_;

  // Tell the label what color it will be drawn onto. It will use whether the
  // background color is opaque or transparent to decide whether to use subpixel
  // rendering. Does not actually set the label's background color.
  title_->SetBackgroundColor(background_color);

  if (is_fullscreen_app_list_enabled_) {
    SetBackground(nullptr);
    SchedulePaint();
    return;
  }

  std::unique_ptr<views::Background> background;
  if (selected_) {
    background_color = kSelectedColor;
    background = views::CreateSolidBackground(background_color);
  } else if (image_shadow_animator_) {
    if (state() == STATE_HOVERED || state() == STATE_PRESSED)
      image_shadow_animator_->animation()->Show();
    else
      image_shadow_animator_->animation()->Hide();
  } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    background_color = kHighlightedColor;
    background = views::CreateSolidBackground(background_color);
  }

  SetBackground(std::move(background));
  SchedulePaint();
}

}  // namespace app_list
