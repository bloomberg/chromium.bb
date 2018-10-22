// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_VIEW_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "components/zoom/zoom_event_manager.h"
#include "components/zoom/zoom_event_manager_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

class Browser;
class FindBarIcon;
class ZoomView;

class PageActionIconContainerView : public views::View,
                                    public PageActionIconContainer,
                                    public zoom::ZoomEventManagerObserver {
 public:
  PageActionIconContainerView(
      const std::vector<PageActionIconType>& types_enabled,
      int icon_size,
      int between_icon_spacing,
      Browser* browser,
      PageActionIconView::Delegate* page_action_icon_delegate,
      LocationBarView::Delegate* location_bar_delegate);
  ~PageActionIconContainerView() override;

  PageActionIconView* GetPageActionIconView(PageActionIconType type);

  // Updates the visual state of all enabled page action icons.
  void UpdateAll();

  // Activates the first visible but inactive icon for accessibility. Returns
  // whether any icons were activated.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // Update the icons color, must be called before painting.
  void SetIconColor(SkColor icon_color);

  // See comment in browser_window.h for more info.
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // PageActionIconContainer:
  void UpdatePageActionIcon(PageActionIconType type) override;

 private:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // ZoomEventManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  ZoomView* zoom_view_ = nullptr;
  FindBarIcon* find_bar_icon_ = nullptr;
  std::vector<PageActionIconView*> page_action_icons_;

  ScopedObserver<zoom::ZoomEventManager, zoom::ZoomEventManagerObserver>
      zoom_observer_;

  DISALLOW_COPY_AND_ASSIGN(PageActionIconContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_CONTAINER_VIEW_H_
