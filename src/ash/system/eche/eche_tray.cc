// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/components/multidevice/logging/logging.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "components/account_id/account_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The icon size should be smaller than the tray item size to avoid the icon
// padding becoming negative.
constexpr int kIconSize = 24;

// This is how much the icon shrinks to give space for the spinner to go
// around it.
constexpr int kIconShrinkSizeForSpinner = 4;

constexpr int kHeaderHeight = 40;
constexpr int kHeaderHorizontalInteriorMargins = 0;
constexpr auto kHeaderDefaultSpacing = gfx::Insets::VH(0, 6);

constexpr auto kBubblePadding = gfx::Insets::VH(8, 8);

constexpr float kDefaultAspectRatio = 16.0 / 9.0f;
constexpr gfx::Size kDefaultBubbleSize(360, 360 * kDefaultAspectRatio);

// Max percentage of the screen height that can be covered by the eche bubble.
constexpr float kMaxHeightPercentage = 0.85;

// Unload timeout to close Eche Bubble in case error from Ech web during closing
constexpr base::TimeDelta kUnloadTimeoutDuration = base::Milliseconds(500);

// Creates a button with the given callback, icon, and tooltip text.
// `message_id` is the resource id of the tooltip text of the icon.
std::unique_ptr<views::Button> CreateButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int message_id) {
  SkColor color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  SkColor disabled_color = SkColorSetA(color, gfx::kDisabledControlAlpha);
  auto button = views::CreateVectorImageButton(std::move(callback));
  views::SetImageFromVectorIconWithColor(button.get(), icon, color,
                                         disabled_color);
  button->SetTooltipText(l10n_util::GetStringUTF16(message_id));
  button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(button.get());

  return button;
}

}  // namespace

EcheTray::EcheTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  const int icon_padding = (kTrayItemSize - kIconSize) / 2;

  icon_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(icon_padding, icon_padding)));

  // Observers setup
  // Note: `ScreenLayoutObserver` starts observing at its constructor.
  observed_session_.Observe(Shell::Get()->session_controller());
  icon_->SetTooltipText(GetAccessibleNameForTray());
  shelf_observation_.Observe(shelf);
  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
  shell_observer_.Observe(Shell::Get());
}

EcheTray::~EcheTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
}

bool EcheTray::IsInitialized() const {
  return GetBubbleWidget() != nullptr;
}

void EcheTray::ClickedOutsideBubble() {
  //  Do nothing
}

std::u16string EcheTray::GetAccessibleNameForTray() {
  // TODO(nayebi): Change this based on the final model of interaction
  // between phone hub and Eche.
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void EcheTray::HandleLocaleChange() {
  icon_->SetTooltipText(GetAccessibleNameForTray());
}

void EcheTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HideBubble();
}

void EcheTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void EcheTray::Initialize() {
  TrayBackgroundView::Initialize();

  // By default the icon is not visible until Eche notification is clicked on.
  SetVisiblePreferred(false);
}

void EcheTray::CloseBubble() {
  if (bubble_)
    HideBubble();
}

void EcheTray::ShowBubble() {
  if (!bubble_)
    return;
  SetIconVisibility(true);
  StopLoadingAnimation();

  bubble_->GetBubbleWidget()->Show();
  bubble_->GetBubbleWidget()->Activate();
  bubble_->bubble_view()->SetVisible(true);
  SetIsActive(true);
  web_view_->GetInitiallyFocusedView()->RequestFocus();
}

bool EcheTray::PerformAction(const ui::Event& event) {
  // Simply toggle between visible/invisibvle
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    HideBubble();
  } else {
    ShowBubble();
  }
  return true;
}

TrayBubbleView* EcheTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* EcheTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

std::u16string EcheTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool EcheTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void EcheTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void EcheTray::OnStreamStatusChanged(eche_app::mojom::StreamStatus status) {
  switch (status) {
    case eche_app::mojom::StreamStatus::kStreamStatusStarted:
      ShowBubble();
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusStopped:
      PurgeAndClose();
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusInitializing:
      // Ignores initializing stream status.
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusUnknown:
      PA_LOG(WARNING) << "Unexpected stream status";
      break;
  }
}

void EcheTray::OnLockStateChanged(bool locked) {
  if (bubble_ && locked)
    PurgeAndClose();
}

void EcheTray::SetUrl(const GURL& url) {
  if (web_view_ && url_ != url)
    web_view_->Navigate(url);
  url_ = url;
}

void EcheTray::SetIcon(const gfx::Image& icon,
                       const std::u16string& tooltip_text) {
  views::ImageButton* icon_view = GetIcon();
  if (icon_view) {
    icon_view->SetImage(
        views::ImageButton::STATE_NORMAL,
        gfx::ImageSkiaOperations::CreateResizedImage(
            icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kIconSize, kIconSize)));
    icon_view->SetTooltipText(tooltip_text);
    SetIconVisibility(true);
  }
}

void EcheTray::LoadBubble(const GURL& url,
                          const gfx::Image& icon,
                          const std::u16string& visible_name) {
  SetUrl(url);
  SetIcon(icon, /*tooltip_text=*/visible_name);
  // If the bubble is already initialized, setting the icon and url was enough
  // to navigate the bubble to the new address.
  if (IsInitialized()) {
    ShowBubble();
    return;
  }
  InitBubble();
  StartLoadingAnimation();
  auto* phone_hub_tray = GetPhoneHubTray();
  if (phone_hub_tray) {
    phone_hub_tray->SetEcheIconActivationCallback(
        base::BindRepeating(&EcheTray::PerformAction, base::Unretained(this)));
  }
  // Hide bubble first until the streaming is ready.
  HideBubble();
}

void EcheTray::PurgeAndClose() {
  StopLoadingAnimation();
  SetIconVisibility(false);

  if (!bubble_)
    return;

  auto* bubble_view = bubble_->GetBubbleView();
  if (bubble_view)
    bubble_view->ResetDelegate();

  SetIsActive(false);
  SetVisiblePreferred(false);
  web_view_ = nullptr;
  close_button_ = nullptr;
  minimize_button_ = nullptr;
  unload_timer_.reset();
  bubble_.reset();
}

void EcheTray::SetGracefulCloseCallback(
    GracefulCloseCallback graceful_close_callback) {
  if (!graceful_close_callback)
    return;
  graceful_close_callback_ = std::move(graceful_close_callback);
}

void EcheTray::StartGracefulClose() {
  if (!graceful_close_callback_) {
    PurgeAndClose();
    return;
  }
  HideBubble();
  std::move(graceful_close_callback_).Run();
  // Graceful close will let Eche Web to close connection release then notify
  // back to native code to close window. In case there is any exception happens
  // in js layer, start a timer to force close widget in case unload can't be
  // finished.
  if (!unload_timer_) {
    unload_timer_ = std::make_unique<base::DelayTimer>(
        FROM_HERE, kUnloadTimeoutDuration, this, &EcheTray::PurgeAndClose);
    unload_timer_->Reset();
  }
}

void EcheTray::HideBubble() {
  SetIsActive(false);
  bubble_->bubble_view()->SetVisible(false);
  bubble_->GetBubbleWidget()->Deactivate();
  bubble_->GetBubbleWidget()->Hide();
}

void EcheTray::InitBubble() {
  base::UmaHistogramEnumeration(
      "Eche.StreamEvent",
      eche_app::mojom::StreamStatus::kStreamStatusInitializing);
  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  // Note: The container id must be smaller than `kShellWindowId_ShelfContainer`
  // in order to let the notifications be shown on top of the eche window.
  init_params.parent_window = Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_AlwaysOnTopContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = GetAnchor();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  const gfx::Size eche_size = CalculateSizeForEche();
  init_params.preferred_width = eche_size.width();
  init_params.close_on_deactivate = false;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.reroute_event_handler = false;
  init_params.corner_radius = kTrayItemCornerRadius;

  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetCanActivate(true);
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  auto* header_view = bubble_view->AddChildView(CreateBubbleHeaderView());
  // The layer is needed to draw the header non-opaquely that is needed to
  // match the phone hub behavior.
  header_view->SetPaintToLayer();
  header_view->layer()->SetFillsBoundsOpaquely(false);

  AshWebView::InitParams params;
  params.can_record_media = true;
  auto web_view = AshWebViewFactory::Get()->Create(params);
  web_view->SetPreferredSize(eche_size);
  if (!url_.is_empty())
    web_view->Navigate(url_);
  web_view_ = bubble_view->AddChildView(std::move(web_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view.release(),
                                                /*event_handling=*/false);

  SetIsActive(true);
  bubble_->GetBubbleView()->UpdateBubble();
}

gfx::Size EcheTray::CalculateSizeForEche() const {
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              tray_container()->GetWidget()->GetNativeWindow())
          .work_area();
  float height_scale =
      (static_cast<float>(work_area_bounds.height()) * kMaxHeightPercentage) /
      kDefaultBubbleSize.height();
  height_scale = std::min(height_scale, 1.0f);
  return gfx::ScaleToFlooredSize(kDefaultBubbleSize, height_scale);
}

void EcheTray::OnArrowBackActivated() {
  if (web_view_)
    web_view_->GoBack();
}

std::unique_ptr<views::View> EcheTray::CreateBubbleHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::VH(0, kHeaderHorizontalInteriorMargins))
      .SetCollapseMargins(false)
      .SetMinimumCrossAxisSize(kHeaderHeight)
      .SetDefault(views::kMarginsKey, kHeaderDefaultSpacing)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Add arrowback button
  header->AddChildView(
      CreateButton(base::BindRepeating(&EcheTray::OnArrowBackActivated,
                                       weak_factory_.GetWeakPtr()),
                   kEcheArrowBackIcon, IDS_APP_ACCNAME_BACK));

  views::Label* title = header->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  title->SetMultiLine(true);
  title->SetAllowCharacterBreak(true);
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Add minimize button
  minimize_button_ = header->AddChildView(CreateButton(
      base::BindRepeating(&EcheTray::CloseBubble, weak_factory_.GetWeakPtr()),
      kEcheMinimizeIcon, IDS_APP_ACCNAME_MINIMIZE));

  // Add close button
  close_button_ = header->AddChildView(
      CreateButton(base::BindRepeating(&EcheTray::StartGracefulClose,
                                       weak_factory_.GetWeakPtr()),
                   kEcheCloseIcon, IDS_APP_ACCNAME_CLOSE));

  return header;
}

views::Button* EcheTray::GetMinimizeButtonForTesting() const {
  return minimize_button_;
}

views::Button* EcheTray::GetCloseButtonForTesting() const {
  return close_button_;
}

views::ImageButton* EcheTray::GetIcon() {
  PhoneHubTray* phone_hub_tray = GetPhoneHubTray();
  if (!phone_hub_tray)
    return nullptr;
  return phone_hub_tray->eche_icon_view();
}

void EcheTray::ResizeIcon(int offset_dip) {
  views::ImageButton* icon_view = GetIcon();
  if (icon_view) {
    auto icon = icon_view->GetImage(views::ImageButton::STATE_NORMAL);
    icon_view->SetImage(
        views::ImageButton::STATE_NORMAL,
        gfx::ImageSkiaOperations::CreateResizedImage(
            icon, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(kIconSize - offset_dip, kIconSize - offset_dip)));
    GetPhoneHubTray()->tray_container()->UpdateLayout();
  }
}

void EcheTray::StopLoadingAnimation() {
  ResizeIcon(0);
  auto* loading_indicator = GetLoadingIndicator();
  if (loading_indicator && loading_indicator->GetAnimating()) {
    loading_indicator->SetAnimating(false);
  }
}

void EcheTray::StartLoadingAnimation() {
  ResizeIcon(kIconShrinkSizeForSpinner);
  auto* loading_indicator = GetLoadingIndicator();
  if (loading_indicator) {
    loading_indicator->SetAnimating(true);
  }
}

void EcheTray::SetIconVisibility(bool visibility) {
  auto* icon = GetIcon();
  if (!icon)
    return;
  icon->SetVisible(visibility);
  GetPhoneHubTray()->tray_container()->UpdateLayout();
}

PhoneHubTray* EcheTray::GetPhoneHubTray() {
  return shelf()->GetStatusAreaWidget()->phone_hub_tray();
}

EcheIconLoadingIndicatorView* EcheTray::GetLoadingIndicator() {
  PhoneHubTray* phone_hub_tray = GetPhoneHubTray();
  if (!phone_hub_tray)
    return nullptr;
  return phone_hub_tray->eche_loading_indicator();
}

void EcheTray::UpdateBubbleBounds() {
  if (!bubble_)
    return;
  bubble_->GetBubbleView()->ChangeAnchorRect(GetAnchor());
}

void EcheTray::OnDisplayConfigurationChanged() {
  UpdateBubbleBounds();
}

void EcheTray::OnAutoHideStateChanged(ShelfAutoHideState state) {
  UpdateBubbleBounds();
}

void EcheTray::OnShelfIconPositionsChanged() {
  UpdateBubbleBounds();
}

void EcheTray::OnTabletModeStarted() {
  UpdateBubbleBounds();
}

void EcheTray::OnTabletModeEnded() {
  UpdateBubbleBounds();
}
void EcheTray::OnShelfAlignmentChanged(aura::Window* root_window,
                                       ShelfAlignment old_alignment) {
  UpdateBubbleBounds();
}

gfx::Rect EcheTray::GetAnchor() {
  return shelf()->GetSystemTrayAnchorRect();
}

BEGIN_METADATA(EcheTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
