// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <algorithm>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/caption_buttons/frame_back_button.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/touch_uma.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace {

// Color for the window title text.
constexpr SkColor kNormalWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
constexpr SkColor kIncognitoWindowTitleTextColor = SK_ColorWHITE;

// The indicator for teleported windows has 8 DIPs before and below it.
constexpr int kProfileIndicatorPadding = 8;

bool IsV1AppBackButtonEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableV1AppBackButton);
}

// Returns true if |window| is currently snapped in split view mode.
bool IsSnappedInSplitView(const aura::Window* window) {
  ash::SplitViewState state = ash::SplitViewNotifier::Get()->GetCurrentState();
  ash::WindowStateType type = window->GetProperty(ash::kWindowStateTypeKey);
  switch (state) {
    case ash::SplitViewState::kNoSnap:
      return false;
    case ash::SplitViewState::kLeftSnapped:
      return type == ash::WindowStateType::kLeftSnapped;
    case ash::SplitViewState::kRightSnapped:
      return type == ash::WindowStateType::kRightSnapped;
    case ash::SplitViewState::kBothSnapped:
      return type == ash::WindowStateType::kLeftSnapped ||
             type == ash::WindowStateType::kRightSnapped;
  }

  NOTREACHED();
  return false;
}

// Returns true if the header should be painted so that it looks the same as
// the header used for packaged apps.
bool UsePackagedAppHeaderStyle(const Browser* browser) {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps.
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         browser->is_app();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  ash::wm::InstallResizeHandleWindowTargeterForWindow(frame->GetNativeWindow());
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
  browser_view()->browser()->command_controller()->RemoveCommandObserver(
      IDC_BACK, this);

  if (TabletModeClient::Get())
    TabletModeClient::Get()->RemoveObserver(this);

  ash::SplitViewNotifier::Get()->RemoveObserver(this);

  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void BrowserNonClientFrameViewAsh::Init() {
  caption_button_container_ = new ash::FrameCaptionButtonContainerView(frame());
  caption_button_container_->UpdateCaptionButtonState(false /*=animate*/);
  AddChildView(caption_button_container_);

  Browser* browser = browser_view()->browser();

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  UpdateProfileIcons();

  aura::Window* window = frame()->GetNativeWindow();
  window->SetProperty(
      aura::client::kAppType,
      static_cast<int>(browser->is_app() ? ash::AppType::CHROME_APP
                                         : ash::AppType::BROWSER));

  window_observer_.Add(GetFrameWindow());

  // To preserve privacy, tag incognito windows so that they won't be included
  // in screenshot sent to assistant server.
  if (browser->profile()->IsOffTheRecord())
    window->SetProperty(ash::kBlockedForAssistantSnapshotKey, true);

  // TabletModeClient may not be initialized during unit tests.
  if (TabletModeClient::Get())
    TabletModeClient::Get()->AddObserver(this);

  if (browser->is_app() && IsV1AppBackButtonEnabled()) {
    browser->command_controller()->AddCommandObserver(IDC_BACK, this);
    back_button_ = new ash::FrameBackButton();
    AddChildView(back_button_);
    // TODO(oshima): Add Tooltip, accessibility name.
  }

  frame_header_ = CreateFrameHeader();

  if (browser_view()->IsBrowserTypeHostedApp())
    SetUpForHostedApp();

  browser_view()->immersive_mode_controller()->AddObserver(this);
  ash::SplitViewNotifier::Get()->AddObserver(this);

  UpdateFrameColors();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForTabStripRegion(
    const views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  const int left_inset = GetTabStripLeftInset();
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return gfx::Rect(left_inset, GetTopInset(restored),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewAsh::GetTopInset(bool restored) const {
  // TODO(estade): why do callsites in this class hardcode false for |restored|?

  if (!ShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tapstrip bounds, the top inset should reach this topmost edge.
    const ImmersiveModeController* const immersive_controller =
        browser_view()->immersive_mode_controller();
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }

    // The header isn't painted for restored popup/app windows in overview mode,
    // but the inset is still calculated below, so the overview code can align
    // the window content with a fake header.
    if (!IsInOverviewMode() || frame()->IsFullscreen() ||
        browser_view()->IsTabStripVisible()) {
      return 0;
    }
  }

  Browser* browser = browser_view()->browser();

  int header_height = frame_header_->GetHeaderHeight();
  if (hosted_app_button_container()) {
    header_height =
        std::max(header_height,
                 hosted_app_button_container()->GetPreferredSize().height());
  }
  if (browser_view()->IsTabStripVisible())
    return header_height - browser_view()->GetTabStripHeight();

  return UsePackagedAppHeaderStyle(browser)
             ? header_height
             : caption_button_container_->bounds().bottom();
}

int BrowserNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return BrowserFrameHeaderAsh::GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewAsh::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewAsh::UpdateMinimumSize() {
  gfx::Size min_size = GetMinimumSize();
  aura::Window* frame_window = frame()->GetNativeWindow();
  const gfx::Size* previous_min_size =
      frame_window->GetProperty(aura::client::kMinimumSize);
  if (!previous_min_size || *previous_min_size != min_size) {
    frame_window->SetProperty(aura::client::kMinimumSize,
                              new gfx::Size(min_size));
  }
}

bool BrowserNonClientFrameViewAsh::CanUserExitFullscreen() const {
  return !platform_util::IsBrowserLockedFullscreen(browser_view()->browser());
}

SkColor BrowserNonClientFrameViewAsh::GetCaptionColor(
    ActiveState active_state) const {
  bool active = ShouldPaintAsActive(active_state);

  SkColor active_color =
      views::FrameCaptionButton::GetButtonColor(ash::kDefaultFrameColor);

  // Hosted apps apply a theme color if specified by the extension.
  Browser* browser = browser_view()->browser();
  base::Optional<SkColor> theme_color =
      browser->app_controller()->GetThemeColor();
  if (theme_color)
    active_color = views::FrameCaptionButton::GetButtonColor(*theme_color);

  if (active)
    return active_color;

  // Add the container for extra hosted app buttons (e.g app menu button).
  const float inactive_alpha_ratio =
      views::FrameCaptionButton::GetInactiveButtonColorAlphaRatio();
  return SkColorSetA(active_color, inactive_alpha_ratio * SK_AlphaOPAQUE);
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewAsh.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const int top_inset = GetTopInset(false);
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - top_inset),
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserNonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  int hit_test = ash::FrameBorderNonClientHitTest(this, point);

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized() &&
      !frame()->IsFullscreen()) {
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    gfx::Rect tabstrip_shadow_bounds(browser_view()->tabstrip()->bounds());
    constexpr int kTabShadowHeight = 4;
    tabstrip_shadow_bounds.set_height(kTabShadowHeight);
    if (tabstrip_shadow_bounds.Contains(client_point))
      return HTCAPTION;
  }

  return hit_test;
}

void BrowserNonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                                 SkPath* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewAsh::ResetWindowControls() {
  BrowserNonClientFrameView::ResetWindowControls();
  caption_button_container_->SetVisible(true);
  caption_button_container_->ResetWindowControls();
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    frame_header_->SchedulePaintForTitle();
}

void BrowserNonClientFrameViewAsh::SizeConstraintsChanged() {}

void BrowserNonClientFrameViewAsh::PaintAsActiveChanged(bool active) {
  BrowserNonClientFrameView::PaintAsActiveChanged(active);

  UpdateProfileIcons();

  const bool should_paint_as_active = ShouldPaintAsActive();
  frame_header_->SetPaintAsActive(should_paint_as_active);
}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  const ash::FrameHeader::Mode header_mode =
      ShouldPaintAsActive() ? ash::FrameHeader::MODE_ACTIVE
                            : ash::FrameHeader::MODE_INACTIVE;
  frame_header_->PaintHeader(canvas, header_mode);
}

void BrowserNonClientFrameViewAsh::Layout() {
  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  frame_header_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->IsTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  frame_header_->SetHeaderHeightForPainting(painted_height);

  if (profile_indicator_icon_)
    LayoutProfileIndicator();
  if (hosted_app_button_container()) {
    hosted_app_button_container()->LayoutInContainer(
        0, caption_button_container_->x(), 0, painted_height);
  }

  BrowserNonClientFrameView::Layout();
  UpdateTopViewInset();

  // The top right corner must be occupied by a caption button for easy mouse
  // access. This check is agnostic to RTL layout.
  DCHECK_EQ(caption_button_container_->y(), 0);
  DCHECK_EQ(caption_button_container_->bounds().right(), width());
}

const char* BrowserNonClientFrameViewAsh::GetClassName() const {
  return "BrowserNonClientFrameViewAsh";
}

void BrowserNonClientFrameViewAsh::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTitleBar;
}

gfx::Size BrowserNonClientFrameViewAsh::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width = frame_header_->GetMinimumHeaderWidth();
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->IsTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    const int min_tabstrip_width =
        browser_view()->tabstrip()->GetMinimumSize().width();
    min_width =
        std::max(min_width, min_tabstrip_width + GetTabStripLeftInset() +
                                GetTabStripRightInset());
  }
  return gfx::Size(min_width, min_client_view_size.height());
}

void BrowserNonClientFrameViewAsh::OnThemeChanged() {
  UpdateFrameColors();
  BrowserNonClientFrameView::OnThemeChanged();
}

void BrowserNonClientFrameViewAsh::ChildPreferredSizeChanged(
    views::View* child) {
  if (browser_view()->initialized()) {
    InvalidateLayout();
    frame()->GetRootView()->Layout();
  }
}

///////////////////////////////////////////////////////////////////////////////
// ash::BrowserFrameHeaderAsh::AppearanceProvider:

SkColor BrowserNonClientFrameViewAsh::GetTitleColor() {
  return browser_view()->IsRegularOrGuestSession()
             ? kNormalWindowTitleTextColor
             : kIncognitoWindowTitleTextColor;
}

SkColor BrowserNonClientFrameViewAsh::GetFrameHeaderColor(bool active) {
  return GetFrameColor(active ? kActive : kInactive);
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFrameHeaderImage(bool active) {
  return GetFrameImage(active ? kActive : kInactive);
}

int BrowserNonClientFrameViewAsh::GetFrameHeaderImageYInset() {
  return ThemeProperties::kFrameHeightAboveTabs - GetTopInset(false);
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFrameHeaderOverlayImage(
    bool active) {
  return GetFrameOverlayImage(active ? kActive : kInactive);
}

///////////////////////////////////////////////////////////////////////////////
// ash::mojom::TabletModeClient:

void BrowserNonClientFrameViewAsh::OnTabletModeToggled(bool enabled) {
  if (!enabled && browser_view()->immersive_mode_controller()->IsRevealed()) {
    // Before updating the caption buttons state below (which triggers a
    // relayout), we want to move the caption buttons from the
    // TopContainerView back to this view.
    OnImmersiveRevealEnded();
  }

  const bool should_show_caption_buttons = ShouldShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);
  if (hosted_app_button_container())
    hosted_app_button_container()->SetVisible(should_show_caption_buttons);

  if (enabled) {
    // Enter immersive mode if the feature is enabled and the widget is not
    // already in fullscreen mode. Popups that are not activated but not
    // minimized are still put in immersive mode, since they may still be
    // visible but not activated due to something transparent and/or not
    // fullscreen (ie. fullscreen launcher).
    if (!frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal() &&
        !frame()->IsMinimized()) {
      browser_view()->immersive_mode_controller()->SetEnabled(true);
      return;
    }
  } else {
    // Exit immersive mode if the feature is enabled and the widget is not in
    // fullscreen mode.
    if (!frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal()) {
      browser_view()->immersive_mode_controller()->SetEnabled(false);
      return;
    }
  }

  InvalidateLayout();
  // Can be null in tests.
  if (frame()->client_view())
    frame()->client_view()->InvalidateLayout();
  if (frame()->GetRootView())
    frame()->GetRootView()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// TabIconViewModel:

bool BrowserNonClientFrameViewAsh::ShouldTabIconViewAnimate() const {
  // Hosted apps use their app icon and shouldn't show a throbber.
  if (browser_view()->IsBrowserTypeHostedApp())
    return false;

  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return delegate ? delegate->GetWindowIcon() : gfx::ImageSkia();
}

void BrowserNonClientFrameViewAsh::EnabledStateChangedForCommand(int id,
                                                                 bool enabled) {
  DCHECK_EQ(IDC_BACK, id);
  DCHECK(browser_view()->browser()->is_app());

  if (back_button_)
    back_button_->SetEnabled(enabled);
}

void BrowserNonClientFrameViewAsh::OnSplitViewStateChanged(
    ash::SplitViewState previous_state,
    ash::SplitViewState new_state) {
  OnOverviewOrSplitviewModeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver:

void BrowserNonClientFrameViewAsh::OnWindowDestroying(aura::Window* window) {
  window_observer_.RemoveAll();
}

void BrowserNonClientFrameViewAsh::OnWindowPropertyChanged(aura::Window* window,
                                                           const void* key,
                                                           intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  } else if (key == ash::kIsShowingInOverviewKey) {
    OnOverviewOrSplitviewModeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// ImmersiveModeController::Observer:

void BrowserNonClientFrameViewAsh::OnImmersiveRevealStarted() {
  // The frame caption buttons use ink drop highlights and flood fill effects.
  // They make those buttons paint_to_layer. On immersive mode, the browser's
  // TopContainerView is also converted to paint_to_layer (see
  // ImmersiveModeControllerAsh::OnImmersiveRevealStarted()). In this mode, the
  // TopContainerView is responsible for painting this
  // BrowserNonClientFrameViewAsh (see TopContainerView::PaintChildren()).
  // However, BrowserNonClientFrameViewAsh is a sibling of TopContainerView not
  // a child. As a result, when the frame caption buttons are set to
  // paint_to_layer as a result of an ink drop effect, they will disappear.
  // https://crbug.com/840242. To fix this, we'll make the caption buttons
  // temporarily children of the TopContainerView while they're all painting to
  // their layers.
  auto* container = browser_view()->top_container();
  container->AddChildViewAt(caption_button_container_, 0);
  if (hosted_app_button_container())
    container->AddChildViewAt(hosted_app_button_container(), 0);
  if (back_button_)
    container->AddChildViewAt(back_button_, 0);

  container->Layout();
}

void BrowserNonClientFrameViewAsh::OnImmersiveRevealEnded() {
  AddChildViewAt(caption_button_container_, 0);
  if (hosted_app_button_container())
    AddChildViewAt(hosted_app_button_container(), 0);
  if (back_button_)
    AddChildViewAt(back_button_, 0);
  Layout();
}

void BrowserNonClientFrameViewAsh::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, protected:

void BrowserNonClientFrameViewAsh::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  BrowserNonClientFrameView::OnProfileAvatarChanged(profile_path);
  UpdateProfileIcons();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, private:

bool BrowserNonClientFrameViewAsh::ShouldShowCaptionButtons() const {
  // In tablet mode, to prevent accidental taps of the window controls, and to
  // give more horizontal space for tabs and the new tab button especially in
  // splitscreen view, we hide the window controls. We only do this when the
  // Home Launcher feature is enabled, since it gives the user the ability to
  // minimize all windows when pressing the Launcher button on the shelf.
  const bool hide_caption_buttons_in_tablet_mode =
      !UsePackagedAppHeaderStyle(browser_view()->browser());
  if (hide_caption_buttons_in_tablet_mode && TabletModeClient::Get() &&
      TabletModeClient::Get()->tablet_mode_enabled()) {
    return false;
  }

  return !IsInOverviewMode() || IsSnappedInSplitView(GetFrameWindow());
}

int BrowserNonClientFrameViewAsh::GetTabStripLeftInset() const {
  return profile_indicator_icon_
             ? kProfileIndicatorPadding + profile_indicator_icon_->width()
             : 0;
}

int BrowserNonClientFrameViewAsh::GetTabStripRightInset() const {
  return ShouldShowCaptionButtons()
             ? caption_button_container_->GetPreferredSize().width()
             : 0;
}

bool BrowserNonClientFrameViewAsh::ShouldPaint() const {
  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_mode_controller->IsEnabled())
    return immersive_mode_controller->IsRevealed();

  if (frame()->IsFullscreen())
    return false;

  // Do not paint for V1 apps in overview mode.
  return browser_view()->IsBrowserTypeNormal() || !IsInOverviewMode();
}

void BrowserNonClientFrameViewAsh::OnOverviewOrSplitviewModeChanged() {
  const bool should_show_caption_buttons = ShouldShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  if (hosted_app_button_container())
    hosted_app_button_container()->SetVisible(should_show_caption_buttons);

  // The entire frame should be repainted for v1 apps, since its visibility can
  // change (see also ShouldPaint()). Do not invoke this on normal browser
  // windows since it does not have to repaint frame except for the caption
  // buttons and repainting might cause stuttering of the animation. See
  // https://crbug.com/949227.
  if (!browser_view()->IsBrowserTypeNormal())
    SchedulePaint();
}

std::unique_ptr<ash::FrameHeader>
BrowserNonClientFrameViewAsh::CreateFrameHeader() {
  std::unique_ptr<ash::FrameHeader> header;
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle(browser)) {
    header = std::make_unique<BrowserFrameHeaderAsh>(frame(), this, this,
                                                     caption_button_container_);
  } else {
    header = std::make_unique<ash::DefaultFrameHeader>(
        frame(), this, caption_button_container_);
  }

  header->SetBackButton(back_button_);
  header->SetLeftHeaderView(window_icon_);
  return header;
}

void BrowserNonClientFrameViewAsh::SetUpForHostedApp() {
  Browser* browser = browser_view()->browser();
  if (!browser->app_controller()->ShouldShowHostedAppButtonContainer())
    return;

  // Add the container for extra hosted app buttons (e.g app menu button).
  set_hosted_app_button_container(new HostedAppButtonContainer(
      frame(), browser_view(), GetCaptionColor(kActive),
      GetCaptionColor(kInactive)));
  AddChildView(hosted_app_button_container());
}

void BrowserNonClientFrameViewAsh::UpdateFrameColors() {
  aura::Window* window = frame()->GetNativeWindow();
  base::Optional<SkColor> active_color, inactive_color;
  if (!UsePackagedAppHeaderStyle(browser_view()->browser())) {
    active_color = GetFrameColor(kActive);
    inactive_color = GetFrameColor(kInactive);
  } else if (browser_view()->IsBrowserTypeHostedApp()) {
    active_color = browser_view()->browser()->app_controller()->GetThemeColor();
  } else if (!browser_view()->browser()->is_app()) {
    active_color =
        base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)
            ? gfx::kGoogleGrey050
            : SkColorSetARGB(0xff, 0x25, 0x4f, 0xae);
  }

  if (active_color) {
    window->SetProperty(ash::kFrameActiveColorKey, *active_color);
    window->SetProperty(ash::kFrameInactiveColorKey,
                        inactive_color.value_or(*active_color));
  } else {
    window->ClearProperty(ash::kFrameActiveColorKey);
    window->ClearProperty(ash::kFrameInactiveColorKey);
  }

  frame_header_->UpdateFrameColors();
}

void BrowserNonClientFrameViewAsh::UpdateTopViewInset() {
  // In immersive fullscreen mode, the top view inset property should be 0.
  const bool immersive =
      browser_view()->immersive_mode_controller()->IsEnabled();
  const bool tab_strip_visible = browser_view()->IsTabStripVisible();
  const int inset =
      (tab_strip_visible || immersive) ? 0 : GetTopInset(/*restored=*/false);
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, inset);
}

bool BrowserNonClientFrameViewAsh::ShouldShowProfileIndicatorIcon() const {
  // We only show the profile indicator for the teleported browser windows
  // between multi-user sessions. Note that you can't teleport an incognito
  // window.
  Browser* browser = browser_view()->browser();
  if (browser->profile()->IsIncognitoProfile())
    return false;

  if (!browser->is_type_tabbed() && !browser->is_app())
    return false;

  return MultiUserWindowManagerHelper::ShouldShowAvatar(
      browser_view()->GetNativeWindow());
}

void BrowserNonClientFrameViewAsh::UpdateProfileIcons() {
  View* root_view = frame()->GetRootView();
  if (ShouldShowProfileIndicatorIcon()) {
    bool needs_layout = !profile_indicator_icon_;
    if (!profile_indicator_icon_) {
      profile_indicator_icon_ = new ProfileIndicatorIcon();
      AddChildView(profile_indicator_icon_);
    }

    gfx::Image image(
        GetAvatarImageForContext(browser_view()->browser()->profile()));
    profile_indicator_icon_->SetSize(image.Size());
    profile_indicator_icon_->SetIcon(image);

    if (needs_layout && root_view) {
      // Adding a child does not invalidate the layout.
      InvalidateLayout();
      root_view->Layout();
    }
  } else if (profile_indicator_icon_) {
    delete profile_indicator_icon_;
    profile_indicator_icon_ = nullptr;
    if (root_view)
      root_view->Layout();
  }
}

void BrowserNonClientFrameViewAsh::LayoutProfileIndicator() {
  DCHECK(profile_indicator_icon_);
  const int frame_height =
      GetTopInset(false) + browser_view()->GetTabStripHeight();
  profile_indicator_icon_->SetPosition(
      gfx::Point(kProfileIndicatorPadding,
                 (frame_height - profile_indicator_icon_->height()) / 2));
  profile_indicator_icon_->SetVisible(true);

  // The layout size is set along with the image.
  DCHECK_LE(profile_indicator_icon_->height(), frame_height);
}

bool BrowserNonClientFrameViewAsh::IsInOverviewMode() const {
  return GetFrameWindow()->GetProperty(ash::kIsShowingInOverviewKey);
}

const aura::Window* BrowserNonClientFrameViewAsh::GetFrameWindow() const {
  return const_cast<BrowserNonClientFrameViewAsh*>(this)->GetFrameWindow();
}

aura::Window* BrowserNonClientFrameViewAsh::GetFrameWindow() {
  return frame()->GetNativeWindow();
}
