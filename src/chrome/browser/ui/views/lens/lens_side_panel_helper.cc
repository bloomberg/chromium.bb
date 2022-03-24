// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_helper.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"
#include "ui/views/widget/widget.h"

namespace lens {

void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  if (!browser_view->lens_side_panel_controller())
    browser_view->CreateLensSidePanelController();
  browser_view->lens_side_panel_controller()->OpenWithURL(url_params);
}

views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback) {
  views::View* anchor =
      BrowserView::GetBrowserViewForBrowser(browser)->top_container();
  return views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<LensRegionSearchInstructionsView>(
          anchor, std::move(close_callback), std::move(escape_callback)));
}

}  // namespace lens
