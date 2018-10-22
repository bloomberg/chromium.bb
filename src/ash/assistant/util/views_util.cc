// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/views_util.h"

#include <memory>

#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

constexpr float kHighlightOpacity = 0.08f;
constexpr int kInkDropInset = 2;

// Helpers ---------------------------------------------------------------------

class DefaultImageButton : public views::ImageButton {
 public:
  explicit DefaultImageButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {}
  ~DefaultImageButton() override = default;

  // views::View:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(
            SkColorSetA(GetInkDropBaseColor(), 0xff * kHighlightOpacity),
            size().width() / 2 - kInkDropInset));
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::RoundRectInkDropMask>(
        size(), gfx::Insets(kInkDropInset), size().width() / 2);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), gfx::Insets(kInkDropInset), GetInkDropCenterBasedOnLastEvent(),
        GetInkDropBaseColor(), ink_drop_visible_opacity());
  }
};

}  // namespace

views::ImageButton* CreateButton(views::ButtonListener* listener,
                                 int size_in_dip,
                                 base::Optional<int> accessible_name_id) {
  constexpr SkColor kInkDropBaseColor = SK_ColorBLACK;
  constexpr float kInkDropVisibleOpacity = 0.06f;

  auto* button = new DefaultImageButton(listener);

  if (accessible_name_id.has_value()) {
    button->SetAccessibleName(
        l10n_util::GetStringUTF16(accessible_name_id.value()));
  }

  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);

  button->SetInkDropMode(views::Button::InkDropMode::ON);
  button->set_has_ink_drop_action_on_click(true);
  button->set_ink_drop_base_color(kInkDropBaseColor);
  button->set_ink_drop_visible_opacity(kInkDropVisibleOpacity);

  button->SetFocusForPlatform();
  button->SetFocusPainter(views::Painter::CreateSolidRoundRectPainter(
      SkColorSetA(kInkDropBaseColor, 0xff * kHighlightOpacity),
      size_in_dip / 2 - kInkDropInset, gfx::Insets(kInkDropInset)));

  button->SetPreferredSize(gfx::Size(size_in_dip, size_in_dip));

  return button;
}

views::ImageButton* CreateImageButton(views::ButtonListener* listener,
                                      const gfx::VectorIcon& icon,
                                      int size_in_dip,
                                      int icon_size_in_dip,
                                      int accessible_name_id,
                                      SkColor icon_color) {
  auto* button = CreateButton(listener, size_in_dip, accessible_name_id);

  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, icon_size_in_dip, icon_color));

  return button;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
