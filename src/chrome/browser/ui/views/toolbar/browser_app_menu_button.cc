// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/ui/in_product_help/in_product_help.h"

namespace {

// Button background and icon colors for in-product help promos. The first is
// the preferred color, but the selected color depends on the
// background. TODO(collinbaker): consider moving these into theme system.
constexpr SkColor kFeaturePromoHighlightDarkColor = gfx::kGoogleBlue600;
constexpr SkColor kFeaturePromoHighlightDarkExtremeColor = gfx::kGoogleBlue900;
constexpr SkColor kFeaturePromoHighlightLightColor = gfx::kGoogleGrey100;
constexpr SkColor kFeaturePromoHighlightLightExtremeColor = SK_ColorWHITE;

// Cycle duration of ink drop pulsing animation used for in-product help.
constexpr base::TimeDelta kFeaturePromoPulseDuration =
    base::TimeDelta::FromMilliseconds(800);

// Max inset for pulsing animation.
constexpr float kFeaturePromoPulseInsetDip = 3.0f;

// An InkDropMask used to animate the size of the BrowserAppMenuButton's ink
// drop. This is used when showing in-product help.
class PulsingInkDropMask : public gfx::AnimationDelegate,
                           public views::InkDropMask {
 public:
  PulsingInkDropMask(views::View* layer_container,
                     const gfx::Size& layer_size,
                     const gfx::Insets& margins,
                     float normal_corner_radius,
                     float max_inset)
      : views::InkDropMask(layer_size),
        layer_container_(layer_container),
        margins_(margins),
        normal_corner_radius_(normal_corner_radius),
        max_inset_(max_inset),
        throb_animation_(this) {
    throb_animation_.SetThrobDuration(
        kFeaturePromoPulseDuration.InMilliseconds());
    throb_animation_.StartThrobbing(-1);
  }

 private:
  // views::InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);

    ui::PaintRecorder recorder(context, layer()->size());

    gfx::RectF bounds(layer()->bounds());
    bounds.Inset(margins_);

    const float current_inset =
        throb_animation_.CurrentValueBetween(0.0f, max_inset_);
    bounds.Inset(gfx::InsetsF(current_inset));
    const float corner_radius = normal_corner_radius_ - current_inset;

    recorder.canvas()->DrawRoundRect(bounds, corner_radius, flags);
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK_EQ(animation, &throb_animation_);
    layer()->SchedulePaint(gfx::Rect(layer()->size()));

    // This is a workaround for crbug.com/935808: for scale factors >1,
    // invalidating the mask layer doesn't cause the whole layer to be repainted
    // on screen. TODO(crbug.com/935808): remove this workaround once the bug is
    // fixed.
    layer_container_->SchedulePaint();
  }

  // The View that contains the InkDrop layer we're masking. This must outlive
  // our instance.
  views::View* const layer_container_;

  // Margins between the layer bounds and the visible ink drop. We use this
  // because sometimes the View we're masking is larger than the ink drop we
  // want to show.
  const gfx::Insets margins_;

  // Normal corner radius of the ink drop without animation. This is also the
  // corner radius at the largest instant of the animation.
  const float normal_corner_radius_;

  // Max inset, used at the smallest instant of the animation.
  const float max_inset_;

  gfx::ThrobAnimation throb_animation_;
};

}  // namespace
#endif

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(toolbar_view), toolbar_view_(toolbar_view) {
  SetInkDropMode(InkDropMode::ON);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  md_observer_.Add(ui::MaterialDesignController::GetInstance());

  UpdateBorder();
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;

  int message_id;
  if (type_and_severity.severity == AppMenuIconController::Severity::NONE) {
    message_id = IDS_APPMENU_TOOLTIP;
  } else if (type_and_severity.type ==
             AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    message_id = IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE;
  } else {
    message_id = IDS_APPMENU_TOOLTIP_ALERT;
  }
  SetTooltipText(l10n_util::GetStringUTF16(message_id));
  UpdateIcon();
}

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
void BrowserAppMenuButton::SetPromoFeature(
    base::Optional<InProductHelpFeature> promo_feature) {
  if (promo_feature_ == promo_feature)
    return;

  promo_feature_ = promo_feature;

  // We override GetInkDropBaseColor() and CreateInkDropMask(), returning the
  // promo values if we are showing an in-product help promo. Calling
  // HostSizeChanged() will force the new mask and color to be fetched.
  //
  // TODO(collinbaker): Consider adding explicit way to recreate mask instead of
  // relying on HostSizeChanged() to do so.
  GetInkDrop()->HostSizeChanged(size());

  views::InkDropState next_state;
  if (promo_feature_ || IsMenuShowing()) {
    // If we are showing a promo, we must use the ACTIVATED state to show the
    // highlight. Otherwise, if the menu is currently showing, we need to keep
    // the ink drop in the ACTIVATED state.
    next_state = views::InkDropState::ACTIVATED;
  } else {
    // If we are not showing a promo and the menu is hidden, we use the
    // DEACTIVATED state.
    next_state = views::InkDropState::DEACTIVATED;
    // TODO(collinbaker): this is brittle since we don't know if something else
    // should keep this ACTIVATED or in some other state. Consider adding code
    // to track the correct state and restore to that.
  }
  GetInkDrop()->AnimateToState(next_state);

  UpdateIcon();
  SchedulePaint();
}
#endif

void BrowserAppMenuButton::ShowMenu(bool for_drop) {
  if (IsMenuShowing())
    return;

#if defined(OS_CHROMEOS)
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_visible())
    keyboard_client->HideKeyboard(ash::mojom::HideReason::kSystem);
#endif

  Browser* browser = toolbar_view_->browser();
  bool alert_reopen_tab_items = false;
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  alert_reopen_tab_items = promo_feature_ == InProductHelpFeature::kReopenTab;
#endif
  base::TimeTicks menu_open_time = base::TimeTicks::Now();
  RunMenu(
      std::make_unique<AppMenuModel>(toolbar_view_, browser,
                                     toolbar_view_->app_menu_icon_controller()),
      browser, for_drop ? AppMenu::FOR_DROP : AppMenu::NO_FLAGS,
      alert_reopen_tab_items);

  if (!for_drop) {
    // Record the time-to-action for the menu. We don't record in the case of a
    // drag-and-drop command because menus opened for drag-and-drop don't block
    // the message loop.
    UMA_HISTOGRAM_TIMES("Toolbar.AppMenuTimeToAction",
                        base::TimeTicks::Now() - menu_open_time);
  }
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateIcon();
}

void BrowserAppMenuButton::UpdateIcon() {
  SkColor severity_color = gfx::kPlaceholderColor;

  const ui::NativeTheme* native_theme = GetNativeTheme();
  switch (type_and_severity_.severity) {
    case AppMenuIconController::Severity::NONE:
      severity_color = GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
      if (promo_feature_)
        severity_color = GetPromoHighlightColor();
#endif
      break;
    case AppMenuIconController::Severity::LOW:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityLow);
      break;
    case AppMenuIconController::Severity::MEDIUM:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityMedium);
      break;
    case AppMenuIconController::Severity::HIGH:
      severity_color = native_theme->GetSystemColor(
          ui::NativeTheme::kColorId_AlertSeverityHigh);
      break;
  }

  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  const gfx::VectorIcon* icon_id = nullptr;
  switch (type_and_severity_.type) {
    case AppMenuIconController::IconType::NONE:
      icon_id = touch_ui ? &kBrowserToolsTouchIcon : &kBrowserToolsIcon;
      DCHECK_EQ(AppMenuIconController::Severity::NONE,
                type_and_severity_.severity);
      break;
    case AppMenuIconController::IconType::UPGRADE_NOTIFICATION:
      icon_id =
          touch_ui ? &kBrowserToolsUpdateTouchIcon : &kBrowserToolsUpdateIcon;
      break;
    case AppMenuIconController::IconType::GLOBAL_ERROR:
      icon_id =
          touch_ui ? &kBrowserToolsErrorTouchIcon : &kBrowserToolsErrorIcon;
      break;
  }

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(*icon_id, severity_color));
}

void BrowserAppMenuButton::SetTrailingMargin(int margin) {
  gfx::Insets* const internal_padding = GetProperty(views::kInternalPaddingKey);
  if (internal_padding->right() == margin)
    return;
  internal_padding->set_right(margin);
  UpdateBorder();
  InvalidateLayout();
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateIcon();
  UpdateBorder();
  PreferredSizeChanged();
}

const char* BrowserAppMenuButton::GetClassName() const {
  return "BrowserAppMenuButton";
}

void BrowserAppMenuButton::UpdateBorder() {
  gfx::Insets new_insets = GetLayoutInsets(TOOLBAR_BUTTON) +
                           *GetProperty(views::kInternalPaddingKey);
  if (!border() || border()->GetInsets() != new_insets)
    SetBorder(views::CreateEmptyBorder(new_insets));
}

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
SkColor BrowserAppMenuButton::GetPromoHighlightColor() const {
  return ToolbarButton::AdjustHighlightColorForContrast(
      GetThemeProvider(), kFeaturePromoHighlightDarkColor,
      kFeaturePromoHighlightLightColor, kFeaturePromoHighlightDarkExtremeColor,
      kFeaturePromoHighlightLightExtremeColor);
}
#endif

gfx::Rect BrowserAppMenuButton::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  gfx::Insets insets =
      GetToolbarInkDropInsets(this, *GetProperty(views::kInternalPaddingKey));
  // If the button is extended, don't inset the trailing edge. The anchored menu
  // should extend to the screen edge as well so the menu is easier to hit
  // (Fitts's law).
  // TODO(pbos): Make sure the button is aware of that it is being extended or
  // not (margin_trailing_ cannot be used as it can be 0 in fullscreen on
  // Touch). When this is implemented, use 0 as a replacement for
  // margin_trailing_ in fullscreen only. Always keep the rest.
  insets.Set(insets.top(), 0, insets.bottom(), 0);
  bounds.Inset(insets);
  return bounds;
}

bool BrowserAppMenuButton::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool BrowserAppMenuButton::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserAppMenuButton::CanDrop(const ui::OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data,
                                        toolbar_view_->browser()->profile());
}

void BrowserAppMenuButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!weak_factory_.HasWeakPtrs());
  if (!g_open_app_immediately_for_testing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BrowserAppMenuButton::ShowMenu,
                       weak_factory_.GetWeakPtr(), true),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowMenu(true);
  }
}

int BrowserAppMenuButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserAppMenuButton::OnDragExited() {
  weak_factory_.InvalidateWeakPtrs();
}

int BrowserAppMenuButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

std::unique_ptr<views::InkDropHighlight>
BrowserAppMenuButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

std::unique_ptr<views::InkDropMask> BrowserAppMenuButton::CreateInkDropMask()
    const {
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  if (promo_feature_) {
    // This gets the latest ink drop insets. |SetTrailingMargin()| is called
    // whenever our margins change (i.e. due to the window maximizing or
    // minimizing) and updates our internal padding property accordingly.
    const gfx::Insets ink_drop_insets =
        GetToolbarInkDropInsets(this, *GetProperty(views::kInternalPaddingKey));
    const float corner_radius =
        (height() - ink_drop_insets.top() - ink_drop_insets.bottom()) / 2.0f;
    return std::make_unique<PulsingInkDropMask>(ink_drop_container(), size(),
                                                ink_drop_insets, corner_radius,
                                                kFeaturePromoPulseInsetDip);
  }
#endif

  return AppMenuButton::CreateInkDropMask();
}

SkColor BrowserAppMenuButton::GetInkDropBaseColor() const {
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  if (promo_feature_)
    return GetPromoHighlightColor();
#endif
  return AppMenuButton::GetInkDropBaseColor();
}
