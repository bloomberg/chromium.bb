// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class WebView;
}  // namespace views

class BrowserView;
class SidePanel;
class ToolbarButton;

// Responsible for managing the WebContents hosted in the browser's side panel
// for Side Search in addition to managing the state of the side panel itself.
class SideSearchBrowserController
    : public SideSearchTabContentsHelper::Delegate,
      public content::WebContentsObserver,
      public views::ViewObserver {
 public:
  enum SideSearchViewID {
    VIEW_ID_NONE = 0,
    VIEW_ID_SIDE_PANEL_CLOSE_BUTTON,
  };

  SideSearchBrowserController(SidePanel* side_panel, BrowserView* browser_view);
  SideSearchBrowserController(const SideSearchBrowserController&) = delete;
  SideSearchBrowserController& operator=(const SideSearchBrowserController&) =
      delete;
  ~SideSearchBrowserController() override;

  // SideSearchTabContentsHelper::Delegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void SidePanelAvailabilityChanged(bool should_close) override;
  void OpenSidePanel() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;

  void UpdateSidePanelForContents(content::WebContents* new_contents,
                                  content::WebContents* old_contents);

  std::unique_ptr<ToolbarButton> CreateToolbarButton();

  views::WebView* web_view_for_testing() { return web_view_; }

  bool GetSidePanelToggledOpen() const;

 private:
  // Gets and sets the toggled state of the side panel. If called with
  // kSideSearchStatePerTab enabled this determines whether the side panel
  // should be open for the currently active tab.
  void SetSidePanelToggledOpen(bool toggled_open);

  // Toggles panel visibility on side panel toolbar button press.
  void SidePanelButtonPressed();

  // Closes side panel on close button press.
  void SidePanelCloseButtonPressed();

  void CloseSidePanel(
      absl::optional<SideSearchCloseActionType> action = absl::nullopt);

  // Called when the side panel is toggled into the closed state. Clears the
  // side panel contents for all tabs belonging to the side panel's browser
  // window.
  void ClearSideContentsCacheForBrowser();

  // Clears the side contents for the currently active tab in this browser
  // window.
  void ClearSideContentsCacheForActiveTab();

  // Updates the `side_panel_`'s visibility and updates it to host the side
  // contents associated with the currently active tab for this browser window.
  void UpdateSidePanel();

  // Callback invoked when the `web_view_`'s visibility state has changed.
  // Visibility changes happens after we update the visibility of the
  // `side_panel_` and a Layout() occurs. This can cause the side panel's layout
  // manager to update the visibility of its web_view_ child.
  void OnWebViewVisibilityChanged();

  base::CallbackListSubscription web_view_visibility_subscription_;

  // The toggled state of the side panel (i.e. the state of the side panel
  // as controlled by the toolbar button).
  bool toggled_open_ = false;

  ToolbarButton* toolbar_button_;
  SidePanel* const side_panel_;
  BrowserView* const browser_view_;
  views::WebView* const web_view_;

  // Tracks and stores the last focused view which is not the
  // `side_panel_` or any of its children. Used to restore focus once
  // the `side_panel_` is hidden.
  views::ExternalFocusTracker focus_tracker_;

  // Observation on `browser_view_` used to track focus manager changes.
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  base::WeakPtrFactory<SideSearchBrowserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_BROWSER_CONTROLLER_H_
