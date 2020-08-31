// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_

#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "ui/views/window/non_client_view.h"

class BrowserFrame;
class BrowserView;
class WebAppFrameToolbarView;

// Type used for functions whose return values depend on the active state of
// the frame.
enum class BrowserFrameActiveState {
  kUseCurrent,  // Use current frame active state.
  kActive,      // Treat frame as active regardless of current state.
  kInactive,    // Treat frame as inactive regardless of current state.
};

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView,
                                  public ProfileAttributesStorage::Observer,
                                  public TabStripObserver {
 public:
  // The minimum total height users should have to use as a drag handle to move
  // the window with.
  static constexpr int kMinimumDragHeight = 8;

  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameView() override;

  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Called when BrowserView creates all it's child views.
  virtual void OnBrowserViewInitViewsComplete();

  // Called on Mac after the browser window is fullscreened or unfullscreened.
  virtual void OnFullscreenStateChanged();

  // Returns whether the caption buttons are drawn at the leading edge (i.e. the
  // left in LTR mode, or the right in RTL mode).
  virtual bool CaptionButtonsOnLeadingEdge() const;

  // Retrieves the bounds in non-client view coordinates within which the
  // TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStripRegion(
      const views::View* tabstrip) const = 0;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  // If |restored| is true, this is calculated as if the window was restored,
  // regardless of its current state.
  virtual int GetTopInset(bool restored) const = 0;

  // Returns the amount that the theme background should be inset.
  virtual int GetThemeBackgroundXInset() const = 0;

  // Updates the top UI state to be hidden or shown in fullscreen according to
  // the preference's state. Currently only used on Mac.
  virtual void UpdateFullscreenTopUI();

  // Returns whether the top UI should hide.
  virtual bool ShouldHideTopUIForFullscreen() const;

  // Returns whether the user is allowed to exit fullscreen on their own (some
  // special modes lock the user in fullscreen).
  virtual bool CanUserExitFullscreen() const;

  // Determines whether the top frame is condensed vertically, as when the
  // window is maximized. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const;

  // Returns whether the shapes of background tabs are visible against the
  // frame, given an active state of |active|.
  virtual bool HasVisibleBackgroundTabShapes(
      BrowserFrameActiveState active_state) const;

  // Returns whether the shapes of background tabs are visible against the frame
  // for either active or inactive windows.
  bool EverHasVisibleBackgroundTabShapes() const;

  // Returns whether tab strokes can be drawn.
  virtual bool CanDrawStrokes() const;

  // Returns the color to use for text, caption buttons, and other title bar
  // elements.
  virtual SkColor GetCaptionColor(BrowserFrameActiveState active_state) const;

  // Returns the color of the browser frame, which is also the color of the
  // tabstrip background.
  SkColor GetFrameColor(BrowserFrameActiveState active_state =
                            BrowserFrameActiveState::kUseCurrent) const;

  // Called by BrowserView to signal the frame color has changed and needs
  // to be repainted.
  virtual void UpdateFrameColor();

  // Returns COLOR_TOOLBAR_TOP_SEPARATOR[,_INACTIVE] depending on the activation
  // state of the window.
  SkColor GetToolbarTopSeparatorColor() const;

  // For non-transparent windows, returns the background tab image resource ID
  // if the image has been customized, directly or indirectly, by the theme.
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

  // Provided for platform-specific updates of minimum window size.
  virtual void UpdateMinimumSize();

  // views::NonClientFrameView:
  using views::NonClientFrameView::ShouldPaintAsActive;
  void Layout() override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  int NonClientHitTest(const gfx::Point& point) override;
  void ResetWindowControls() override;

  WebAppFrameToolbarView* web_app_frame_toolbar_for_testing() {
    return web_app_frame_toolbar_;
  }

 protected:
  // Converts an ActiveState to a bool representing whether the frame should be
  // treated as active.
  bool ShouldPaintAsActive(BrowserFrameActiveState active_state) const;

  // Compute aspects of the frame needed to paint the frame background.
  gfx::ImageSkia GetFrameImage(BrowserFrameActiveState active_state =
                                   BrowserFrameActiveState::kUseCurrent) const;
  gfx::ImageSkia GetFrameOverlayImage(
      BrowserFrameActiveState active_state =
          BrowserFrameActiveState::kUseCurrent) const;

  // views::NonClientFrameView:
  void ChildPreferredSizeChanged(views::View* child) override;
  void PaintAsActiveChanged(bool active) override;
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;

  void set_web_app_frame_toolbar(
      WebAppFrameToolbarView* web_app_frame_toolbar) {
    web_app_frame_toolbar_ = web_app_frame_toolbar;
  }
  WebAppFrameToolbarView* web_app_frame_toolbar() {
    return web_app_frame_toolbar_;
  }
  const WebAppFrameToolbarView* web_app_frame_toolbar() const {
    return web_app_frame_toolbar_;
  }

 private:
  // views::NonClientFrameView:
#if defined(OS_WIN)
  int GetSystemMenuY() const override;
#endif

  // Get the |frame_| theme provider since it should be non-null even before
  // we're added to the view hierarchy.
  const ui::ThemeProvider* GetFrameThemeProvider() const;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // Menu button and page status icons. Only used by web-app windows.
  WebAppFrameToolbarView* web_app_frame_toolbar_ = nullptr;

  ScopedObserver<TabStrip, TabStripObserver> tab_strip_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameView);
};

namespace chrome {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
