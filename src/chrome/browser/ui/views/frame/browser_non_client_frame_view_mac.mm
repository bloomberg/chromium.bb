// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/window_controls_overlay_input_routing_mac.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"

namespace {

constexpr int kFrameExtraPaddingForWindowControlsOverlay = 10;
constexpr int kFramePaddingLeft = 75;
// Keep in sync with web_app_frame_toolbar_browsertest.cc
constexpr double kTitlePaddingWidthFraction = 0.1;

FullscreenToolbarStyle GetUserPreferredToolbarStyle(bool always_show) {
  // In Kiosk mode, we don't show top Chrome UI.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return FullscreenToolbarStyle::TOOLBAR_NONE;
  return always_show ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                     : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  show_fullscreen_toolbar_.Init(
      prefs::kShowFullscreenToolbar, browser_view->GetProfile()->GetPrefs(),
      base::BindRepeating(&BrowserNonClientFrameViewMac::UpdateFullscreenTopUI,
                          base::Unretained(this)));
  if (!base::FeatureList::IsEnabled(features::kImmersiveFullscreen)) {
    fullscreen_toolbar_controller_.reset(
        [[FullscreenToolbarController alloc] initWithBrowserView:browser_view]);
    [fullscreen_toolbar_controller_
        setToolbarStyle:GetUserPreferredToolbarStyle(
                            *show_fullscreen_toolbar_)];
  }

  if (browser_view->GetIsWebAppType()) {
    if (browser_view->browser()->app_controller()) {
      set_web_app_frame_toolbar(AddChildView(
          std::make_unique<WebAppFrameToolbarView>(frame, browser_view)));

      if (browser_view->IsWindowControlsOverlayEnabled()) {
        caption_button_placeholder_container_ =
            AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());

        AddRoutingForWindowControlsOverlayViews();
      }
    }

    // The window title appears above the web app frame toolbar (if present),
    // which surrounds the title with minimal-ui buttons on the left,
    // and other controls (such as the app menu button) on the right.
    if (browser_view->ShouldShowWindowTitle()) {
      window_title_ = AddChildView(
          std::make_unique<views::Label>(browser_view->GetWindowTitle()));
      window_title_->SetID(VIEW_ID_WINDOW_TITLE);
    }
  }
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
  if ([fullscreen_toolbar_controller_ isInFullscreen])
    [fullscreen_toolbar_controller_ exitFullscreenMode];
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

void BrowserNonClientFrameViewMac::OnFullscreenStateChanged() {
  if (base::FeatureList::IsEnabled(features::kImmersiveFullscreen)) {
    browser_view()->immersive_mode_controller()->SetEnabled(
        browser_view()->IsFullscreen());
    return;
  }
  if (browser_view()->IsFullscreen()) {
    ToggleWebAppFrameToolbarViewVisibility();

    [fullscreen_toolbar_controller_ enterFullscreenMode];
  } else {
    // Exiting tab fullscreen requires updating Top UI.
    // Called from here so we can capture exiting tab fullscreen both by
    // pressing 'ESC' key and by clicking green traffic light button.
    UpdateFullscreenTopUI();
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  browser_view()->Layout();
}

bool BrowserNonClientFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  // In OSX 10.11, caption buttons always get drawn on the left side of the
  // browser frame instead of the leading edge. This causes a discrepancy in
  // RTL mode.
  return !base::i18n::IsRTL() || base::mac::IsAtLeastOS10_12();
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  // TODO(weili): In the future, we should hide the title bar, and show the
  // tab strip directly under the menu bar. For now, just lay our content
  // under the native title bar. Use the default title bar height to avoid
  // calling through private APIs.
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  gfx::Rect bounds(0, GetTopInset(restored), width(),
                   tabstrip_minimum_size.height());

  // Do not draw caption buttons on fullscreen.
  if (!frame()->IsFullscreen()) {
    const int kCaptionWidth = base::mac::IsAtMostOS10_15() ? 70 : 85;
    if (CaptionButtonsOnLeadingEdge())
      bounds.Inset(gfx::Insets(0, kCaptionWidth, 0, 0));
    else
      bounds.Inset(gfx::Insets(0, 0, 0, kCaptionWidth));
  }

  return bounds;
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  if (web_app_frame_toolbar()) {
    DCHECK(browser_view()->GetIsWebAppType());
    if (ShouldHideTopUIForFullscreen())
      return 0;
    return web_app_frame_toolbar()->GetPreferredSize().height() +
           kWebAppMenuMargin * 2;
  }

  if (!browser_view()->GetTabStripVisible())
    return 0;

  // Mac seems to reserve 1 DIP of the top inset as a resize handle.
  constexpr int kResizeHandleHeight = 1;
  constexpr int kTabstripTopInset = 8;
  int top_inset = kTabstripTopInset;
  if (EverHasVisibleBackgroundTabShapes()) {
    top_inset =
        std::max(top_inset, BrowserNonClientFrameView::kMinimumDragHeight +
                                kResizeHandleHeight);
  }

  // Calculate the y offset for the tab strip because in fullscreen mode the tab
  // strip may need to move under the slide down menu bar.
  CGFloat y_offset = TopUIFullscreenYOffset();
  if (y_offset > 0) {
    // When menubar shows up, we need to update mouse tracking area.
    NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
    NSRect content_bounds = [[window contentView] bounds];
    // Backing bar tracking area uses native coordinates.
    CGFloat tracking_height =
        FullscreenBackingBarHeight() + top_inset + y_offset;
    NSRect backing_bar_area =
        NSMakeRect(0, NSMaxY(content_bounds) - tracking_height,
                   NSWidth(content_bounds), tracking_height);
    [fullscreen_toolbar_controller_ updateToolbarFrame:backing_bar_area];
  }

  return y_offset + top_inset;
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateFullscreenTopUI() {
  if (base::FeatureList::IsEnabled(features::kImmersiveFullscreen))
    return;

  FullscreenToolbarStyle old_style =
      [fullscreen_toolbar_controller_ toolbarStyle];

  // Update to the new toolbar style if needed.
  FullscreenToolbarStyle new_style;
  FullscreenController* controller =
      browser_view()->GetExclusiveAccessManager()->fullscreen_controller();
  if ((controller->IsWindowFullscreenForTabOrPending() ||
       controller->IsExtensionFullscreenOrPending())) {
    browser_view()->HideDownloadShelf();
    new_style = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    new_style = GetUserPreferredToolbarStyle(*show_fullscreen_toolbar_);
    browser_view()->UnhideDownloadShelf();
  }
  [fullscreen_toolbar_controller_ setToolbarStyle:new_style];

  ToggleWebAppFrameToolbarViewVisibility();

  if (![fullscreen_toolbar_controller_ isInFullscreen] ||
      old_style == new_style)
    return;

  // Notify browser that top ui state has been changed so that we can update
  // the bookmark bar state as well.
  browser_view()->browser()->FullscreenTopUIStateChanged();

  // Re-layout if toolbar style changes in fullscreen mode.
  if (frame()->IsFullscreen()) {
    browser_view()->Layout();
    // The web frame toolbar is visible in fullscreen mode on Mac and thus
    // requires a re-layout when in fullscreen and shown.
    if (web_app_frame_toolbar() && !ShouldHideTopUIForFullscreen())
      InvalidateLayout();
  }
}

bool BrowserNonClientFrameViewMac::ShouldHideTopUIForFullscreen() const {
  if (frame()->IsFullscreen()) {
    return [fullscreen_toolbar_controller_ toolbarStyle] !=
           FullscreenToolbarStyle::TOOLBAR_PRESENT;
  }
  return false;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

void BrowserNonClientFrameViewMac::PaintAsActiveChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::PaintAsActiveChanged();
}

void BrowserNonClientFrameViewMac::UpdateFrameColor() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::UpdateFrameColor();
}

void BrowserNonClientFrameViewMac::OnThemeChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::OnThemeChanged();
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_inset = GetTopInset(false);

  // If the operating system is handling drawing the window titlebar then the
  // titlebar height will not be included in |GetTopInset|, so we have to
  // explicitly add it. If a custom titlebar is being drawn, this calculation
  // will be zero.
  NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  DCHECK(window);
  top_inset += window.frame.size.height -
               [window contentRectForFrameRect:window.frame].size.height;

  return gfx::Rect(client_bounds.x(), client_bounds.y() - top_inset,
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserNonClientFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE)
    return super_component;

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  const int component = frame()->client_view()->NonClientHitTest(point);
  return (component == HTNOWHERE && bounds().Contains(point)) ? HTCAPTION
                                                              : component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 SkPath* window_mask) {}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::UpdateWindowTitle() {
  if (window_title_) {
    DCHECK(browser_view()->GetIsWebAppType());
    window_title_->SetText(browser_view()->GetWindowTitle());
    Layout();
  }
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

void BrowserNonClientFrameViewMac::UpdateMinimumSize() {
  GetWidget()->OnSizeConstraintsChanged();
}

void BrowserNonClientFrameViewMac::WindowControlsOverlayEnabledChanged() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    caption_button_placeholder_container_ =
        AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    UpdateCaptionButtonPlaceholderContainerBackground();

    AddRoutingForWindowControlsOverlayViews();

    caption_buttons_overlay_input_routing_view_->Enable();
    web_app_frame_toolbar_overlay_routing_view_->Enable();
  } else {
    caption_buttons_overlay_input_routing_view_->Disable();
    web_app_frame_toolbar_overlay_routing_view_->Disable();

    RemoveChildView(caption_button_placeholder_container_);
    caption_button_placeholder_container_ = nullptr;

    caption_buttons_overlay_input_routing_view_ = nullptr;
    web_app_frame_toolbar_overlay_routing_view_ = nullptr;
  }

  web_app_frame_toolbar()->OnWindowControlsOverlayEnabledChanged();
  frame()->client_view()->InvalidateLayout();
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  gfx::Size client_size = frame()->client_view()->GetMinimumSize();
  if (browser_view()->browser()->is_type_normal())
    client_size.SetToMax(
        browser_view()->tab_strip_region_view()->GetMinimumSize());

  // macOS apps generally don't allow their windows to get shorter than a
  // certain height, which empirically seems to be related to their *minimum*
  // width rather than their current width. This 4:3 ratio was chosen
  // empirically because it looks decent for both tabbed and untabbed browsers.
  client_size.SetToMax(gfx::Size(0, (client_size.width() * 3) / 4));

  return client_size;
}

void BrowserNonClientFrameViewMac::AddedToWidget() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    caption_buttons_overlay_input_routing_view_->Enable();
    web_app_frame_toolbar_overlay_routing_view_->Enable();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:

void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->GetIsNormalType() &&
      !browser_view()->GetIsWebAppType()) {
    return;
  }

  SkColor frame_color = GetFrameColor();
  canvas->DrawColor(frame_color);

  if (window_title_) {
    window_title_->SetBackgroundColor(frame_color);
    window_title_->SetEnabledColor(
        GetCaptionColor(BrowserFrameActiveState::kUseCurrent));
  }

  auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_view()->browser()->profile());
  if (!theme_service->UsingSystemTheme())
    PaintThemedFrame(canvas);
}

void BrowserNonClientFrameViewMac::Layout() {
  if (browser_view()->IsWindowControlsOverlayEnabled())
    LayoutWindowControlsOverlay();
  else
    LayoutTitleBarForWebApp();
  NonClientFrameView::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

gfx::Rect BrowserNonClientFrameViewMac::GetCenteredTitleBounds(
    int frame_width,
    int frame_height,
    int left_inset_x,
    int right_inset_x,
    int title_width) {
  // Center in container.
  int title_x = (frame_width - title_width) / 2;

  // Align right side to right inset if overlapping.
  title_x = std::min(title_x, right_inset_x - title_width);

  // Align left side to left inset if overlapping.
  title_x = std::max(title_x, left_inset_x);

  // Clip width to right inset if overlapping.
  title_width = std::min(title_width, right_inset_x - title_x);

  return gfx::Rect(title_x, 0, title_width, frame_height);
}

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, TopUIFullscreenYOffset(), width(),
                       image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}

CGFloat BrowserNonClientFrameViewMac::FullscreenBackingBarHeight() const {
  BrowserView* browser_view = this->browser_view();
  DCHECK(browser_view->IsFullscreen());

  CGFloat total_height = 0;
  if (browser_view->GetTabStripVisible())
    total_height += browser_view->GetTabStripHeight();

  if (browser_view->IsToolbarVisible())
    total_height += browser_view->toolbar()->bounds().height();

  return total_height;
}

int BrowserNonClientFrameViewMac::TopUIFullscreenYOffset() const {
  if (!browser_view()->GetTabStripVisible() || !browser_view()->IsFullscreen())
    return 0;

  CGFloat menu_bar_height =
      [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
  CGFloat title_bar_height =
      NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                       styleMask:NSWindowStyleMaskTitled]);
  if (base::FeatureList::IsEnabled(features::kImmersiveFullscreen))
    return menu_bar_height == 0 ? 0 : menu_bar_height + title_bar_height;
  return [[fullscreen_toolbar_controller_ menubarTracker] menubarFraction] *
         (menu_bar_height + title_bar_height);
}

void BrowserNonClientFrameViewMac::LayoutTitleBarForWebApp() {
  if (!web_app_frame_toolbar())
    return;

  const int available_height = GetTopInset(true);
  int leading_x = kFramePaddingLeft;
  int trailing_x = width();

  if (CaptionButtonsOnLeadingEdge() && base::i18n::IsRTL()) {
    leading_x = 0;
    trailing_x = width() - kFramePaddingLeft;
  }

  std::pair<int, int> remaining_bounds =
      web_app_frame_toolbar()->LayoutInContainer(leading_x, trailing_x, 0,
                                                 available_height);
  leading_x = remaining_bounds.first;
  trailing_x = remaining_bounds.second;

  const int title_padding =
      base::checked_cast<int>(std::round(width() * kTitlePaddingWidthFraction));
  window_title_->SetBoundsRect(GetCenteredTitleBounds(
      width(), available_height, leading_x + title_padding,
      trailing_x - title_padding,
      window_title_->CalculatePreferredSize().width()));
}

gfx::Rect BrowserNonClientFrameViewMac::GetWebAppFrameToolbarAvailableBounds(
    bool is_rtl,
    const gfx::Size& frame,
    int y,
    int caption_button_container_width) {
  if (is_rtl) {
    return gfx::Rect(0, 0, frame.width() - caption_button_container_width,
                     frame.height());
  } else {
    return gfx::Rect(caption_button_container_width, 0,
                     frame.width() - caption_button_container_width,
                     frame.height());
  }
}

gfx::Rect BrowserNonClientFrameViewMac::GetCaptionButtonPlaceholderBounds(
    bool is_rtl,
    const gfx::Size& frame,
    int y,
    int width,
    int extra_padding) {
  if (is_rtl)
    return gfx::Rect(frame.width() - width, y, width, frame.height());
  else {
    // Add extra width to caption_button_placeholder_container_overlay_ so the
    // maximize button does not look like it is touching the border of the
    // overlay and there is padding between the two.
    return gfx::Rect(0, y, width + extra_padding, frame.height());
  }
}

void BrowserNonClientFrameViewMac::LayoutWindowControlsOverlay() {
  const bool is_rtl = CaptionButtonsOnLeadingEdge() && base::i18n::IsRTL();
  const gfx::Size frame(width(), GetTopInset(false));
  gfx::Rect caption_button_container_bounds = GetCaptionButtonPlaceholderBounds(
      is_rtl, frame, 0, kFramePaddingLeft,
      kFrameExtraPaddingForWindowControlsOverlay);
  gfx::Rect web_app_frame_toolbar_available_bounds =
      GetWebAppFrameToolbarAvailableBounds(
          is_rtl, frame, 0, caption_button_container_bounds.width());

  // Layout CaptionButtonPlaceholderContainer which would have the traffic
  // lights.
  caption_button_placeholder_container_->SetBoundsRect(
      caption_button_container_bounds);

  // Layout WebAppFrameToolbarView.
  web_app_frame_toolbar()->LayoutForWindowControlsOverlay(
      web_app_frame_toolbar_available_bounds);

  content::WebContents* web_contents = browser_view()->GetActiveWebContents();
  // WebContents can be null when an app window is first launched.
  if (web_contents) {
    const int overlay_width =
        width() - (caption_button_placeholder_container_->size().width() +
                   web_app_frame_toolbar()->size().width());
    gfx::Rect bounding_rect;

    if (CaptionButtonsOnLeadingEdge() && base::i18n::IsRTL()) {
      bounding_rect =
          gfx::Rect(caption_button_placeholder_container_->size().width() +
                        web_app_frame_toolbar()->size().width(),
                    0, overlay_width, GetTopInset(false));
    } else {
      bounding_rect = GetMirroredRect(
          gfx::Rect(caption_button_placeholder_container_->size().width(), 0,
                    overlay_width, GetTopInset(false)));
    }
    web_contents->UpdateWindowControlsOverlay(bounding_rect);
  }
}

void BrowserNonClientFrameViewMac::
    UpdateCaptionButtonPlaceholderContainerBackground() {
  if (caption_button_placeholder_container_) {
    caption_button_placeholder_container_->SetBackground(
        views::CreateSolidBackground(GetFrameColor()));
  }
}

void BrowserNonClientFrameViewMac::AddRoutingForWindowControlsOverlayViews() {
  caption_buttons_overlay_input_routing_view_ =
      std::make_unique<WindowControlsOverlayInputRoutingMac>(
          this, caption_button_placeholder_container_,
          remote_cocoa::mojom::WindowControlsOverlayNSViewType::
              kCaptionButtonContainer);

  web_app_frame_toolbar_overlay_routing_view_ =
      std::make_unique<WindowControlsOverlayInputRoutingMac>(
          this, web_app_frame_toolbar(),
          remote_cocoa::mojom::WindowControlsOverlayNSViewType::
              kWebAppFrameToolbar);
}

void BrowserNonClientFrameViewMac::ToggleWebAppFrameToolbarViewVisibility() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    web_app_frame_toolbar()->SetVisible(!ShouldHideTopUIForFullscreen());
    InvalidateLayout();
  }
}
