// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_SIMULATOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_SIMULATOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/base/page_transition_types.h"

class GURL;
class TabStripModel;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// Helper class for unit tests that rely on more realistically simulated tab
// activity, such as showing and hiding tabs when switching between them.
class TabActivitySimulator {
 public:
  class TestWebContentsObserver;

  TabActivitySimulator();
  ~TabActivitySimulator();

  // Simulates a navigation to |url| using the given transition type.
  void Navigate(content::WebContents* web_contents,
                const GURL& url,
                ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK);

  // Creates a new WebContents suitable for testing.
  std::unique_ptr<content::WebContents> CreateWebContents(
      content::BrowserContext* browser_context,
      bool initially_visible = false);

  // Creates a new WebContents suitable for testing, adds it to the tab strip
  // and commits a navigation to |initial_url|. The WebContents is owned by the
  // TabStripModel, so its tab must be closed later, e.g. via CloseAllTabs().
  content::WebContents* AddWebContentsAndNavigate(
      TabStripModel* tab_strip_model,
      const GURL& initial_url,
      ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK);

  // Sets |new_index| as the active tab in its tab strip, hiding the previously
  // active tab.
  void SwitchToTabAt(TabStripModel* tab_strip_model, int new_index);

 private:
  // Owns the observers we've created.
  std::vector<std::unique_ptr<TestWebContentsObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(TabActivitySimulator);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_ACTIVITY_SIMULATOR_H_
