// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/scoped_multi_source_observation.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/arc/compat_mode/overlay_dialog.h"
#include "components/arc/compat_mode/style/arc_color_provider.h"
#include "components/arc/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/rrect_f.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

// Draws the blue-ish highlight border to the parent view according to the
// highlight path.
class HighlightBorder : public views::View {
 public:
  HighlightBorder() = default;
  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;
  ~HighlightBorder() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    InvalidateLayout();
    SchedulePaint();
  }

  void Layout() override {
    auto bounds = parent()->GetLocalBounds();
    bounds.Inset(gfx::Insets(views::FocusRing::kHaloInset));
    SetBoundsRect(bounds);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    const auto rrect =
        views::HighlightPathGenerator::GetRoundRectForView(parent());
    if (!rrect)
      return;
    auto rect = (*rrect).rect();
    View::ConvertRectToTarget(parent(), this, &rect);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedBorderColor));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(views::FocusRing::kHaloThickness);
    canvas->DrawRoundRect(rect, (*rrect).GetSimpleRadius(), flags);
  }
};

}  // namespace

class ArcSplashScreenDialogView::ArcSplashScreenWindowObserver
    : public aura::WindowObserver {
 public:
  ArcSplashScreenWindowObserver(aura::Window* window,
                                base::RepeatingClosure on_close_callback)
      : on_close_callback_(on_close_callback) {
    window_observation_.Observe(window);
  }

  ArcSplashScreenWindowObserver(const ArcSplashScreenWindowObserver&) = delete;
  ArcSplashScreenWindowObserver& operator=(
      const ArcSplashScreenWindowObserver&) = delete;
  ~ArcSplashScreenWindowObserver() override = default;

 private:
  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != aura::client::kShowStateKey)
      return;

    ui::WindowShowState state =
        window->GetProperty(aura::client::kShowStateKey);
    if (state == ui::SHOW_STATE_FULLSCREEN ||
        state == ui::SHOW_STATE_MAXIMIZED) {
      // Run the callback when window is fullscreen or maximized.
      on_close_callback_.Run();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }

  base::RepeatingClosure on_close_callback_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

ArcSplashScreenDialogView::ArcSplashScreenDialogView(
    base::OnceClosure close_callback,
    aura::Window* parent,
    views::View* anchor,
    bool is_for_unresizable)
    : anchor_(anchor), close_callback_(std::move(close_callback)) {
  const auto background_color = GetDialogBackgroundBaseColor();
  // Setup delegate.
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(parent);
  set_title_margins(gfx::Insets());
  set_margins(gfx::Insets());
  SetAnchorView(anchor_);
  SetTitle(l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE));
  SetShowTitle(false);
  SetAccessibleRole(ax::mojom::Role::kDialog);
  set_color(background_color);
  set_adjust_if_offscreen(false);
  set_close_on_deactivate(false);

  // Setup views.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets(20, 24, 24, 24))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true));

  constexpr gfx::Size kLogoImageSize(152, 126);
  AddChildView(
      views::Builder<views::ImageView>()  // Logo
          .SetImage(gfx::ImageSkiaOperations::ExtractSubset(
              gfx::CreateVectorIcon(kCompatModeSplashscreenIcon,
                                    kLogoImageSize.width(), background_color),
              gfx::Rect(kLogoImageSize)))
          .Build());
  AddChildView(views::Builder<views::Label>()  // Header
                   .SetText(l10n_util::GetStringUTF16(
                       IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE))
                   .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                   .SetHorizontalAlignment(gfx::ALIGN_CENTER)
                   .SetAllowCharacterBreak(true)
                   .SetMultiLine(true)
                   .SetProperty(views::kMarginsKey, gfx::Insets(8, 0))
                   .Build());
  AddChildView(
      views::Builder<views::Label>()  // Body
          .SetText(is_for_unresizable
                       ? l10n_util::GetStringUTF16(
                             IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_BODY_UNRESIZABLE)
                       : l10n_util::GetStringFUTF16(
                             IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_BODY,
                             parent->GetTitle()))
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
          .SetHorizontalAlignment(gfx::ALIGN_CENTER)
          .SetMultiLine(true)
          .Build());
  AddChildView(views::Builder<views::MdTextButton>()  // Close button
                   .CopyAddressTo(&close_button_)
                   .SetCallback(base::BindRepeating(
                       &ArcSplashScreenDialogView::OnCloseButtonClicked,
                       base::Unretained(this)))
                   .SetText(l10n_util::GetStringUTF16(
                       IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_CLOSE))
                   .SetCornerRadius(16)
                   .SetProminent(true)
                   .SetIsDefault(true)
                   .SetProperty(views::kMarginsKey, gfx::Insets(20, 0, 0, 0))
                   .Build());

  // Setup highlight border.
  highlight_border_ =
      anchor_->AddChildView(std::make_unique<HighlightBorder>());

  // Add window observer.
  window_observer_ = std::make_unique<ArcSplashScreenWindowObserver>(
      parent,
      base::BindRepeating(&ArcSplashScreenDialogView::OnCloseButtonClicked,
                          base::Unretained(this)));
}

ArcSplashScreenDialogView::~ArcSplashScreenDialogView() = default;

gfx::Size ArcSplashScreenDialogView::CalculatePreferredSize() const {
  auto width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  const auto* widget = GetWidget();
  if (widget && widget->parent()) {
    constexpr int kHorizontalMarginDp = 32;
    width = std::min(widget->parent()->GetWindowBoundsInScreen().width() -
                         kHorizontalMarginDp * 2,
                     width);
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

void ArcSplashScreenDialogView::AddedToWidget() {
  constexpr int kCornerRadius = 12;
  auto* const frame = GetBubbleFrameView();
  if (frame)
    frame->SetCornerRadius(kCornerRadius);
}

void ArcSplashScreenDialogView::OnCloseButtonClicked() {
  if (!close_callback_)
    return;

  anchor_->RemoveChildViewT(highlight_border_);

  std::move(close_callback_).Run();
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ArcSplashScreenDialogView::Show(aura::Window* parent,
                                     bool is_for_unresizable) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(parent);
  DCHECK(frame_view);
  auto* const anchor_view =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();

  if (!anchor_view) {
    LOG(ERROR) << "Failed to show the compat mode splash screen because the "
                  "center button is missing.";
    return;
  }

  auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
      base::BindOnce(&OverlayDialog::CloseIfAny, base::Unretained(parent)),
      parent, anchor_view, is_for_unresizable);

  OverlayDialog::Show(
      parent,
      base::BindOnce(&ArcSplashScreenDialogView::OnCloseButtonClicked,
                     base::Unretained(dialog_view.get())),
      /*dialog_view=*/nullptr);

  views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view))->Show();
}

}  // namespace arc
