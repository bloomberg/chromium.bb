// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"

#include <algorithm>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/ui/frame/interior_resize_handler_targeter.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// Color for the window title text.
constexpr SkColor kNormalWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
constexpr SkColor kIncognitoWindowTitleTextColor = SK_ColorWHITE;

// The indicator for teleported windows has 8 DIPs before and below it.
constexpr int kProfileIndicatorPadding = 8;

// Returns true if the header should be painted so that it looks the same as
// the header used for packaged apps.
bool UsePackagedAppHeaderStyle(const Browser* browser) {
  if (browser->is_type_normal() ||
      (browser->is_type_popup() && !browser->is_trusted_source())) {
    return false;
  }

  return !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
}

}  // namespace

BrowserNonClientFrameViewChromeOS::BrowserNonClientFrameViewChromeOS(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(
      frame->GetNativeWindow());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  frame->GetNativeWindow()->SetEventTargeter(
      std::make_unique<chromeos::InteriorResizeHandleTargeter>());
#endif
}

BrowserNonClientFrameViewChromeOS::~BrowserNonClientFrameViewChromeOS() {
  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void BrowserNonClientFrameViewChromeOS::Init() {
  caption_button_container_ =
      new chromeos::FrameCaptionButtonContainerView(frame());
  caption_button_container_->UpdateCaptionButtonState(false /*=animate*/);
  AddChildView(caption_button_container_);

  Browser* browser = browser_view()->browser();

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    AddChildView(views::Builder<TabIconView>()
                     .CopyAddressTo(&window_icon_)
                     .SetModel(this)
                     .Build());
  }

  UpdateProfileIcons();

  window_observation_.Observe(GetFrameWindow());

  // To preserve privacy, tag incognito windows so that they won't be included
  // in screenshot sent to assistant server.
  if (browser->profile()->IsOffTheRecord()) {
    frame()->GetNativeWindow()->SetProperty(
        chromeos::kBlockedForAssistantSnapshotKey, true);
  }

  display_observer_.emplace(this);

  if (frame()->ShouldDrawFrameHeader())
    frame_header_ = CreateFrameHeader();

  if (browser_view()->GetIsWebAppType() && !browser->is_type_app_popup()) {
    // Add the container for extra web app buttons (e.g app menu button).
    set_web_app_frame_toolbar(AddChildView(
        std::make_unique<WebAppFrameToolbarView>(frame(), browser_view())));
  }

  browser_view()->immersive_mode_controller()->AddObserver(this);
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  const int left_inset = GetTabStripLeftInset();
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return gfx::Rect(left_inset, GetTopInset(restored),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip_minimum_size.height());
}

int BrowserNonClientFrameViewChromeOS::GetTopInset(bool restored) const {
  // TODO(estade): why do callsites in this class hardcode false for |restored|?

  if (!GetShouldPaint()) {
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
    if (!GetOverviewMode() || frame()->IsFullscreen() ||
        browser_view()->GetTabStripVisible() ||
        browser_view()->webui_tab_strip()) {
      return 0;
    }
  }

  Browser* browser = browser_view()->browser();

  int header_height = frame_header_ ? frame_header_->GetHeaderHeight() : 0;
  if (web_app_frame_toolbar()) {
    header_height = std::max(
        header_height, web_app_frame_toolbar()->GetPreferredSize().height());
  }
  if (browser_view()->GetTabStripVisible())
    return header_height - browser_view()->GetTabStripHeight();

  return UsePackagedAppHeaderStyle(browser)
             ? header_height
             : caption_button_container_->bounds().bottom();
}

int BrowserNonClientFrameViewChromeOS::GetThemeBackgroundXInset() const {
  return BrowserFrameHeaderChromeOS::GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewChromeOS::UpdateFrameColor() {
  OnUpdateFrameColor();
  BrowserNonClientFrameView::UpdateFrameColor();
}

void BrowserNonClientFrameViewChromeOS::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

bool BrowserNonClientFrameViewChromeOS::CanUserExitFullscreen() const {
  return !platform_util::IsBrowserLockedFullscreen(browser_view()->browser());
}

SkColor BrowserNonClientFrameViewChromeOS::GetCaptionColor(
    BrowserFrameActiveState active_state) const {
  bool active = ShouldPaintAsActive(active_state);

  SkColor active_color =
      views::FrameCaptionButton::GetButtonColor(chromeos::kDefaultFrameColor);

  // Web apps apply a theme color if specified by the extension.
  Browser* browser = browser_view()->browser();
  absl::optional<SkColor> theme_color =
      browser->app_controller()->GetThemeColor();
  if (theme_color)
    active_color = views::FrameCaptionButton::GetButtonColor(*theme_color);

  if (active)
    return active_color;

  // Add the container for extra web-app buttons (e.g app menu button).
  const float inactive_alpha_ratio =
      views::FrameCaptionButton::GetInactiveButtonColorAlphaRatio();
  return SkColorSetA(active_color, inactive_alpha_ratio * SK_AlphaOPAQUE);
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewChromeOS.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewChromeOS::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const int top_inset = GetTopInset(false);
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - top_inset),
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserNonClientFrameViewChromeOS::NonClientHitTest(
    const gfx::Point& point) {
  int hit_test = chromeos::FrameBorderNonClientHitTest(this, point);

  // When the window is restored (and not in tablet split-view mode) we want a
  // large click target above the tabs to drag the window, so redirect clicks in
  // the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized() &&
      !frame()->IsFullscreen() &&
      !chromeos::TabletState::Get()->InTabletMode()) {
    // TODO(crbug.com/1213133): Tab Strip hit calculation and bounds logic
    // should reside in the TabStrip class.
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

void BrowserNonClientFrameViewChromeOS::GetWindowMask(const gfx::Size& size,
                                                      SkPath* window_mask) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The opaque region of surface should be set exactly same as the frame header
  // path in BrowserFrameHeader.
  if (frame()->ShouldDrawFrameHeader())
    *window_mask = frame_header_->GetWindowMaskForFrameHeader(size);
#endif
}

void BrowserNonClientFrameViewChromeOS::ResetWindowControls() {
  BrowserNonClientFrameView::ResetWindowControls();
  caption_button_container_->SetVisible(GetShowCaptionButtons());
  caption_button_container_->ResetWindowControls();
}

void BrowserNonClientFrameViewChromeOS::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewChromeOS::UpdateWindowTitle() {
  if (!frame()->IsFullscreen() && frame_header_)
    frame_header_->SchedulePaintForTitle();

  frame()->GetNativeWindow()->SetProperty(
      chromeos::kWindowOverviewTitleKey,
      browser_view()->browser()->GetWindowTitleForCurrentTab(
          /*include_app_name=*/false));
}

void BrowserNonClientFrameViewChromeOS::SizeConstraintsChanged() {}

void BrowserNonClientFrameViewChromeOS::OnPaint(gfx::Canvas* canvas) {
  if (!GetShouldPaint())
    return;

  if (frame_header_)
    frame_header_->PaintHeader(canvas);
}

void BrowserNonClientFrameViewChromeOS::Layout() {
  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  if (frame_header_)
    frame_header_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->GetTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  if (frame_header_)
    frame_header_->SetHeaderHeightForPainting(painted_height);

  if (profile_indicator_icon_)
    LayoutProfileIndicator();
  if (web_app_frame_toolbar()) {
    web_app_frame_toolbar()->LayoutInContainer(GetToolbarLeftInset(),
                                               caption_button_container_->x(),
                                               0, painted_height);
  }

  BrowserNonClientFrameView::Layout();
  UpdateTopViewInset();

  if (frame_header_) {
    // The top right corner must be occupied by a caption button for easy mouse
    // access. This check is agnostic to RTL layout.
    DCHECK_EQ(caption_button_container_->y(), 0);
    DCHECK_EQ(caption_button_container_->bounds().right(), width());
  }
}

void BrowserNonClientFrameViewChromeOS::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTitleBar;
}

gfx::Size BrowserNonClientFrameViewChromeOS::GetMinimumSize() const {
  // System web apps (e.g. Settings) may have a fixed minimum size.
  Browser* browser = browser_view()->browser();
  if (web_app::IsSystemWebApp(browser)) {
    gfx::Size minimum_size = web_app::GetSystemWebAppMinimumWindowSize(browser);
    if (!minimum_size.IsEmpty())
      return minimum_size;
  }

  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width =
      frame_header_ ? frame_header_->GetMinimumHeaderWidth() : 0;
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->GetTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    const int min_tabstrip_width =
        browser_view()->tab_strip_region_view()->GetMinimumSize().width();
    min_width =
        std::max(min_width, min_tabstrip_width + GetTabStripLeftInset() +
                                GetTabStripRightInset());
  }
  return gfx::Size(min_width, min_client_view_size.height());
}

void BrowserNonClientFrameViewChromeOS::OnThemeChanged() {
  OnUpdateFrameColor();
  BrowserNonClientFrameView::OnThemeChanged();
}

void BrowserNonClientFrameViewChromeOS::ChildPreferredSizeChanged(
    views::View* child) {
  if (browser_view()->initialized()) {
    InvalidateLayout();
    frame()->GetRootView()->Layout();
  }
}

bool BrowserNonClientFrameViewChromeOS::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside the frame's bounds.
    return false;
  }

  bool should_leave_to_top_container = false;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // In immersive mode, the caption buttons container is reparented to the
  // TopContainerView and hence |rect| should not be claimed here.  See
  // BrowserNonClientFrameViewChromeOS::OnImmersiveRevealStarted().
  should_leave_to_top_container =
      browser_view()->immersive_mode_controller()->IsRevealed();
#endif

  return !should_leave_to_top_container;
}

SkColor BrowserNonClientFrameViewChromeOS::GetTitleColor() {
  return browser_view()->GetRegularOrGuestSession()
             ? kNormalWindowTitleTextColor
             : kIncognitoWindowTitleTextColor;
}

SkColor BrowserNonClientFrameViewChromeOS::GetFrameHeaderColor(bool active) {
  return GetFrameColor(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

gfx::ImageSkia BrowserNonClientFrameViewChromeOS::GetFrameHeaderImage(
    bool active) {
  return GetFrameImage(active ? BrowserFrameActiveState::kActive
                              : BrowserFrameActiveState::kInactive);
}

int BrowserNonClientFrameViewChromeOS::GetFrameHeaderImageYInset() {
  return ThemeProperties::kFrameHeightAboveTabs - GetTopInset(false);
}

gfx::ImageSkia BrowserNonClientFrameViewChromeOS::GetFrameHeaderOverlayImage(
    bool active) {
  return GetFrameOverlayImage(active ? BrowserFrameActiveState::kActive
                                     : BrowserFrameActiveState::kInactive);
}

void BrowserNonClientFrameViewChromeOS::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInTabletMode:
      OnTabletModeToggled(true);
      return;
    case display::TabletState::kInClamshellMode:
      OnTabletModeToggled(false);
      return;
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void BrowserNonClientFrameViewChromeOS::OnTabletModeToggled(bool enabled) {
  if (!enabled && browser_view()->immersive_mode_controller()->IsRevealed()) {
    // Before updating the caption buttons state below (which triggers a
    // relayout), we want to move the caption buttons from the
    // TopContainerView back to this view.
    OnImmersiveRevealEnded();
  }

  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);
  if (web_app_frame_toolbar())
    web_app_frame_toolbar()->SetVisible(should_show_caption_buttons);

  if (enabled) {
    // Enter immersive mode if the feature is enabled and the widget is not
    // already in fullscreen mode. Popups that are not activated but not
    // minimized are still put in immersive mode, since they may still be
    // visible but not activated due to something transparent and/or not
    // fullscreen (ie. fullscreen launcher).
    if (!frame()->IsFullscreen() && !browser_view()->GetSupportsTabStrip() &&
        !frame()->IsMinimized()) {
      browser_view()->immersive_mode_controller()->SetEnabled(true);
      return;
    }
  } else {
    // Exit immersive mode if the feature is enabled and the widget is not in
    // fullscreen mode.
    if (!frame()->IsFullscreen() && !browser_view()->GetSupportsTabStrip()) {
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

bool BrowserNonClientFrameViewChromeOS::ShouldTabIconViewAnimate() const {
  // Web apps use their app icon and shouldn't show a throbber.
  if (browser_view()->GetIsWebAppType())
    return false;

  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

ui::ImageModel BrowserNonClientFrameViewChromeOS::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return delegate ? delegate->GetWindowIcon() : ui::ImageModel();
}

void BrowserNonClientFrameViewChromeOS::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(window_observation_.IsObserving());
  window_observation_.Reset();
}

void BrowserNonClientFrameViewChromeOS::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == chromeos::kIsShowingInOverviewKey) {
    OnAddedToOrRemovedFromOverview();
    return;
  }

  if (!frame_header_)
    return;

  if (key == aura::client::kShowStateKey) {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  } else if (key == chromeos::kFrameRestoreLookKey) {
    frame_header_->view()->InvalidateLayout();
  }
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveRevealStarted() {
  // The frame caption buttons use ink drop highlights and flood fill effects.
  // They make those buttons paint_to_layer. On immersive mode, the browser's
  // TopContainerView is also converted to paint_to_layer (see
  // ImmersiveModeControllerAsh::OnImmersiveRevealStarted()). In this mode, the
  // TopContainerView is responsible for painting this
  // BrowserNonClientFrameViewChromeOS (see TopContainerView::PaintChildren()).
  // However, BrowserNonClientFrameViewChromeOS is a sibling of TopContainerView
  // not a child. As a result, when the frame caption buttons are set to
  // paint_to_layer as a result of an ink drop effect, they will disappear.
  // https://crbug.com/840242. To fix this, we'll make the caption buttons
  // temporarily children of the TopContainerView while they're all painting to
  // their layers.
  auto* container = browser_view()->top_container();
  container->AddChildViewAt(caption_button_container_, 0);
  if (web_app_frame_toolbar())
    container->AddChildViewAt(web_app_frame_toolbar(), 0);

  container->Layout();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // In Lacros, when entering in immersive fullscreen, it is possible
  // that chromeos::FrameHeader::painted_height_ is set to '0', when
  // Layout() is called. This is because the tapstrip gets hidden.
  //
  // When it happens, PaintFrameImagesInRoundRect() has an empty rect
  // to paint onto, and the TabStrip's new theme is not painted.
  if (frame_header_ && frame_header_->GetHeaderHeightForPainting() == 0)
    frame_header_->LayoutHeader();
#endif
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveRevealEnded() {
  // Ensure the WebAppFrameToolbarView and FrameCaptionButtonContainerView
  // receive events before the BrowserView by appending instead of inserting
  // the child views.
  if (web_app_frame_toolbar())
    AddChildView(web_app_frame_toolbar());
  AddChildView(caption_button_container_);
  Layout();
}

void BrowserNonClientFrameViewChromeOS::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

void BrowserNonClientFrameViewChromeOS::PaintAsActiveChanged() {
  BrowserNonClientFrameView::PaintAsActiveChanged();

  UpdateProfileIcons();

  if (frame_header_)
    frame_header_->SetPaintAsActive(ShouldPaintAsActive());
}

void BrowserNonClientFrameViewChromeOS::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  BrowserNonClientFrameView::OnProfileAvatarChanged(profile_path);
  UpdateProfileIcons();
}

bool BrowserNonClientFrameViewChromeOS::GetShowCaptionButtons() const {
  return GetShowCaptionButtonsWhenNotInOverview() && !GetOverviewMode();
}

bool BrowserNonClientFrameViewChromeOS::GetShowCaptionButtonsWhenNotInOverview()
    const {
  return UsePackagedAppHeaderStyle(browser_view()->browser()) ||
         !chromeos::TabletState::Get()->InTabletMode();
}

int BrowserNonClientFrameViewChromeOS::GetToolbarLeftInset() const {
  // Include padding on left and right of icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding * 2 + profile_indicator_icon_->width()
             : 0;
}

int BrowserNonClientFrameViewChromeOS::GetTabStripLeftInset() const {
  // Include padding on left of icon.
  // The tab strip has its own 'padding' to the right of the icon.
  return profile_indicator_icon_
             ? kProfileIndicatorPadding + profile_indicator_icon_->width()
             : 0;
}

int BrowserNonClientFrameViewChromeOS::GetTabStripRightInset() const {
  int inset = 0;
  if (GetShowCaptionButtonsWhenNotInOverview())
    inset += caption_button_container_->GetPreferredSize().width();
  if (web_app_frame_toolbar())
    inset += web_app_frame_toolbar()->GetPreferredSize().width();
  return inset;
}

bool BrowserNonClientFrameViewChromeOS::GetShouldPaint() const {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // Normal windows that have a WebUI-based tab strip do not need a browser
  // frame as no tab strip is drawn on top of the browser frame.
  if (WebUITabStripContainerView::UseTouchableTabStrip(
          browser_view()->browser()) &&
      browser_view()->GetSupportsTabStrip()) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_mode_controller->IsEnabled())
    return immersive_mode_controller->IsRevealed();

  return !frame()->IsFullscreen();
}

void BrowserNonClientFrameViewChromeOS::OnAddedToOrRemovedFromOverview() {
  const bool should_show_caption_buttons = GetShowCaptionButtons();
  caption_button_container_->SetVisible(should_show_caption_buttons);
  if (web_app_frame_toolbar())
    web_app_frame_toolbar()->SetVisible(should_show_caption_buttons);
}

std::unique_ptr<chromeos::FrameHeader>
BrowserNonClientFrameViewChromeOS::CreateFrameHeader() {
  std::unique_ptr<chromeos::FrameHeader> header;
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle(browser)) {
    header = std::make_unique<BrowserFrameHeaderChromeOS>(
        frame(), this, this, caption_button_container_);
  } else {
    header = std::make_unique<chromeos::DefaultFrameHeader>(
        frame(), this, caption_button_container_);
  }

  header->SetLeftHeaderView(window_icon_);
  return header;
}

void BrowserNonClientFrameViewChromeOS::UpdateTopViewInset() {
  // In immersive fullscreen mode, the top view inset property should be 0.
  const bool immersive =
      browser_view()->immersive_mode_controller()->IsEnabled();
  const bool tab_strip_visible = browser_view()->GetTabStripVisible();
  const int inset =
      (tab_strip_visible || immersive) ? 0 : GetTopInset(/*restored=*/false);
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, inset);
}

bool BrowserNonClientFrameViewChromeOS::GetShowProfileIndicatorIcon() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We only show the profile indicator for the teleported browser windows
  // between multi-user sessions. Note that you can't teleport an incognito
  // window.
  Browser* browser = browser_view()->browser();
  if (browser->profile()->IsIncognitoProfile())
    return false;

  if (browser->is_type_popup())
    return false;

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // TODO(http://crbug.com/1059514): This check shouldn't be necessary.  Provide
  // an appropriate affordance for the profile icon with the webUI tabstrip and
  // remove this block.
  if (!browser_view()->GetTabStripVisible())
    return false;
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  return MultiUserWindowManagerHelper::ShouldShowAvatar(
      browser_view()->GetNativeWindow());
#else
  // Multi-signin support is deprecated in Lacros.
  return false;
#endif
}

void BrowserNonClientFrameViewChromeOS::UpdateProfileIcons() {
  // Multi-signin support is deprecated in Lacros, so only do this for ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  View* root_view = frame()->GetRootView();
  if (GetShowProfileIndicatorIcon()) {
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
#endif
}

void BrowserNonClientFrameViewChromeOS::LayoutProfileIndicator() {
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

bool BrowserNonClientFrameViewChromeOS::GetOverviewMode() const {
  return GetFrameWindow()->GetProperty(chromeos::kIsShowingInOverviewKey);
}

void BrowserNonClientFrameViewChromeOS::OnUpdateFrameColor() {
  aura::Window* window = frame()->GetNativeWindow();
  absl::optional<SkColor> active_color, inactive_color;
  if (!UsePackagedAppHeaderStyle(browser_view()->browser())) {
    active_color = GetFrameColor(BrowserFrameActiveState::kActive);
    inactive_color = GetFrameColor(BrowserFrameActiveState::kInactive);
  } else if (browser_view()->GetIsWebAppType()) {
    active_color = browser_view()->browser()->app_controller()->GetThemeColor();
  } else if (!browser_view()->browser()->is_type_app() &&
             !browser_view()->browser()->is_type_app_popup()) {
    // TODO(crbug.com/836128): Remove when System Web Apps flag is removed, as
    // the above web-app branch will render the theme color.
    active_color = SK_ColorWHITE;
  }

  if (active_color) {
    window->SetProperty(chromeos::kFrameActiveColorKey, *active_color);
    window->SetProperty(chromeos::kFrameInactiveColorKey,
                        inactive_color.value_or(*active_color));
  } else {
    window->ClearProperty(chromeos::kFrameActiveColorKey);
    window->ClearProperty(chromeos::kFrameInactiveColorKey);
  }

  if (frame_header_)
    frame_header_->UpdateFrameColors();
}

const aura::Window* BrowserNonClientFrameViewChromeOS::GetFrameWindow() const {
  return const_cast<BrowserNonClientFrameViewChromeOS*>(this)->GetFrameWindow();
}

aura::Window* BrowserNonClientFrameViewChromeOS::GetFrameWindow() {
  return frame()->GetNativeWindow();
}

BEGIN_METADATA(BrowserNonClientFrameViewChromeOS, BrowserNonClientFrameView)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtons)
ADD_READONLY_PROPERTY_METADATA(bool, ShowCaptionButtonsWhenNotInOverview)
ADD_READONLY_PROPERTY_METADATA(int, ToolbarLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripLeftInset)
ADD_READONLY_PROPERTY_METADATA(int, TabStripRightInset)
ADD_READONLY_PROPERTY_METADATA(bool, ShouldPaint)
ADD_READONLY_PROPERTY_METADATA(bool, ShowProfileIndicatorIcon)
ADD_READONLY_PROPERTY_METADATA(bool, OverviewMode)

END_METADATA
