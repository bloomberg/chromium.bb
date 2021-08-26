// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/browser_frame_header_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"

namespace {
class WebAppNonClientFrameViewAshTest;
}

class ProfileIndicatorIcon;
class TabIconView;

namespace chromeos {
class FrameCaptionButtonContainerView;
}  // namespace chromeos

// Provides the BrowserNonClientFrameView for Chrome OS.
class BrowserNonClientFrameViewChromeOS
    : public BrowserNonClientFrameView,
      public BrowserFrameHeaderChromeOS::AppearanceProvider,
      public display::DisplayObserver,
      public TabIconViewModel,
      public aura::WindowObserver,
      public ImmersiveModeController::Observer {
 public:
  METADATA_HEADER(BrowserNonClientFrameViewChromeOS);
  BrowserNonClientFrameViewChromeOS(BrowserFrame* frame,
                                    BrowserView* browser_view);
  BrowserNonClientFrameViewChromeOS(const BrowserNonClientFrameViewChromeOS&) =
      delete;
  BrowserNonClientFrameViewChromeOS& operator=(
      const BrowserNonClientFrameViewChromeOS&) = delete;
  ~BrowserNonClientFrameViewChromeOS() override;

  void Init();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateFrameColor() override;
  void UpdateThrobber(bool running) override;
  bool CanUserExitFullscreen() const override;
  SkColor GetCaptionColor(BrowserFrameActiveState active_state) const override;
  TabSearchBubbleHost* GetTabSearchBubbleHost() override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
  views::View::Views GetChildrenInZOrder() override;

  // BrowserFrameHeaderChromeOS::AppearanceProvider:
  SkColor GetTitleColor() override;
  SkColor GetFrameHeaderColor(bool active) override;
  gfx::ImageSkia GetFrameHeaderImage(bool active) override;
  int GetFrameHeaderImageYInset() override;
  gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  void OnTabletModeToggled(bool enabled);

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  ui::ImageModel GetFaviconForTabIconView() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;

 protected:
  // BrowserNonClientFrameView:
  void PaintAsActiveChanged() override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  // TODO(pkasting): Test the public API or create a test helper class, don't
  // add this many friends
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                           NonImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeBrowserViewTestNoWebUiTabStrip,
                           ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           ToggleTabletModeRelayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTestNoWebUiTabStrip,
                           AvatarDisplayOnTeleportedWindow);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           BrowserHeaderVisibilityInTabletModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           AppHeaderVisibilityInTabletModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           ImmersiveModeTopViewInset);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           ToggleTabletModeOnMinimizedWindow);
  FRIEND_TEST_ALL_PREFIXES(WebAppNonClientFrameViewAshTest,
                           ActiveStateOfButtonMatchesWidget);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewChromeOSTest,
                           RestoreMinimizedBrowserUpdatesCaption);
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeControllerChromeosWebAppBrowserTest,
                           FrameLayoutToggleTabletMode);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewChromeOSTest,
                           TabletModeBrowserCaptionButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(
      HomeLauncherBrowserNonClientFrameViewChromeOSTest,
      CaptionButtonVisibilityForBrowserLaunchedInTabletMode);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewChromeOSTest,
                           TabletModeAppCaptionButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(NonHomeLauncherBrowserNonClientFrameViewChromeOSTest,
                           HeaderHeightForSnappedBrowserInSplitView);
  FRIEND_TEST_ALL_PREFIXES(TabSearchFrameCaptionButtonTest,
                           TabSearchBubbleHostTest);

  friend class WebAppNonClientFrameViewAshTest;

  // Returns true if GetShowCaptionButtonsWhenNotInOverview() returns true
  // and this browser window is not showing in overview.
  bool GetShowCaptionButtons() const;

  // In tablet mode, to prevent accidental taps of the window controls, and to
  // give more horizontal space for tabs and the new tab button (especially in
  // split view), we hide the window controls even when this browser window is
  // not showing in overview. We only do this when the Home Launcher feature is
  // enabled, because it gives the user the ability to minimize all windows when
  // pressing the Launcher button on the shelf. So, this function returns true
  // if the Home Launcher feature is disabled or we are in clamshell mode.
  bool GetShowCaptionButtonsWhenNotInOverview() const;

  // Distance between the edge of the NonClientFrameView and the web app frame
  // toolbar.
  int GetToolbarLeftInset() const;

  // Distance between the edges of the NonClientFrameView and the tab strip.
  int GetTabStripLeftInset() const;
  int GetTabStripRightInset() const;

  // Returns true if there is anything to paint. Some fullscreen windows do
  // not need their frames painted.
  bool GetShouldPaint() const;

  // Helps to hide or show the header as needed when the window is added to or
  // removed from overview.
  void OnAddedToOrRemovedFromOverview();

  // Creates the frame header for the browser window.
  std::unique_ptr<chromeos::FrameHeader> CreateFrameHeader();

  // Triggers the web-app origin and icon animations, assumes the web-app UI
  // elements exist.
  void StartWebAppAnimation();

  // Updates the kTopViewInset window property after a layout.
  void UpdateTopViewInset();

  // Returns true if |profile_indicator_icon_| should be shown.
  bool GetShowProfileIndicatorIcon() const;

  // Updates the icon that indicates a teleported window.
  void UpdateProfileIcons();

  void LayoutProfileIndicator();

  // Returns whether this window is currently in the overview list.
  bool GetOverviewMode() const;

  // Called any time the frame color may have changed.
  void OnUpdateFrameColor();

  // Returns the top level aura::Window for this browser window.
  const aura::Window* GetFrameWindow() const;
  aura::Window* GetFrameWindow();

  // View which contains the window controls.
  chromeos::FrameCaptionButtonContainerView* caption_button_container_ =
      nullptr;

  TabSearchBubbleHost* tab_search_bubble_host_ = nullptr;

  // For popups, the window icon.
  TabIconView* window_icon_ = nullptr;

  // This is used for teleported windows (in multi-profile mode).
  ProfileIndicatorIcon* profile_indicator_icon_ = nullptr;

  // Helper class for painting the header.
  std::unique_ptr<chromeos::FrameHeader> frame_header_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  absl::optional<display::ScopedDisplayObserver> display_observer_;

  base::WeakPtrFactory<BrowserNonClientFrameViewChromeOS> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_H_
