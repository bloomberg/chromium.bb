// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

// Integration tests for interactions between TabStripModel and TabStrip.
class TabStripBrowsertest : public InProcessBrowserTest {
 public:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  TabStrip* tab_strip() {
    return BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, true); }

  tab_groups::TabGroupId AddTabToNewGroup(int tab_index) {
    tab_strip_model()->AddToNewGroup({tab_index});
    return tab_strip_model()->GetTabGroupForTab(tab_index).value();
  }

  void AddTabToExistingGroup(int tab_index, tab_groups::TabGroupId group) {
    tab_strip_model()->AddToExistingGroup({tab_index}, group);
  }

  std::vector<content::WebContents*> GetWebContentses() {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->tab_count(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(i));
    return contentses;
  }

  std::vector<content::WebContents*> GetWebContentsesInOrder(
      const std::vector<int>& order) {
    std::vector<content::WebContents*> contentses;
    for (int i = 0; i < tab_strip()->tab_count(); ++i)
      contentses.push_back(tab_strip_model()->GetWebContentsAt(order[i]));
    return contentses;
  }
};

// Regression test for crbug.com/983961.
IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabAndDeleteGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(2);

  Tab* tab0 = tab_strip()->tab_at(0);
  Tab* tab1 = tab_strip()->tab_at(1);
  Tab* tab2 = tab_strip()->tab_at(2);

  tab_strip_model()->AddToExistingGroup({2}, group);

  EXPECT_EQ(tab0, tab_strip()->tab_at(0));
  EXPECT_EQ(tab2, tab_strip()->tab_at(1));
  EXPECT_EQ(tab1, tab_strip()->tab_at(2));

  EXPECT_EQ(group, tab_strip_model()->GetTabGroupForTab(1));

  std::vector<tab_groups::TabGroupId> groups =
      tab_strip_model()->group_model()->ListTabGroups();
  EXPECT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabLeft_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabLeft_AddsToGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(2)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabLeft_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabLeft_ShiftsBetweenGroups) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabLeft_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabLeft_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabLeft(tab_strip()->tab_at(1));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabRight_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 0, 2});
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabRight_AddsToGroup) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be added to the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabRight_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from the group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(1)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabRight_ShiftsBetweenGroups) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  tab_groups::TabGroupId group = AddTabToNewGroup(1);

  // Instead of moving, the tab should be removed from its old group, then added
  // to the new group.
  const auto expected = GetWebContentsesInOrder({0, 1, 2});
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
  EXPECT_EQ(tab_strip()->tab_at(0)->group().value(), group);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftTabRight_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(2));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftTabRight_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftTabRight(tab_strip()->tab_at(0));
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_DoesNotAddToGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(0);
  AddTabToNewGroup(1);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);

  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(0)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(0));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabFirst_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       MoveTabFirst_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({2, 0, 1, 3});
  tab_strip()->MoveTabFirst(tab_strip()->tab_at(2));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Success) {
  AppendTab();
  AppendTab();

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(0));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_MovePinnedTab_Success) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1, 3});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Success) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto expected = GetWebContentsesInOrder({0, 2, 1});
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_DoesNotAddToGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_RemovesFromGroup) {
  AppendTab();
  AppendTab();

  AddTabToNewGroup(1);
  AddTabToNewGroup(2);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);

  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  EXPECT_EQ(tab_strip()->tab_at(2)->group(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_NoPinnedTabs_Failure) {
  AppendTab();
  AppendTab();

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_PinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(1));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, MoveTabLast_AllPinnedTabs_Failure) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);

  const auto contentses = GetWebContentses();
  tab_strip()->MoveTabLast(tab_strip()->tab_at(2));
  // No changes expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto expected = GetWebContentsesInOrder({1, 2, 0});
  tab_strip()->ShiftGroupLeft(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_OtherGroup) {
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupLeft(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupLeft_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupLeft_Failure_Pinned) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(0, true);

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupLeft(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_Success) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group);

  const auto expected = GetWebContentsesInOrder({2, 0, 1});
  tab_strip()->ShiftGroupRight(group);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest, ShiftGroupRight_OtherGroup) {
  AppendTab();
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group1 = AddTabToNewGroup(0);
  AddTabToExistingGroup(1, group1);

  tab_groups::TabGroupId group2 = AddTabToNewGroup(2);
  AddTabToExistingGroup(3, group2);

  const auto expected = GetWebContentsesInOrder({2, 3, 0, 1});
  tab_strip()->ShiftGroupRight(group1);
  EXPECT_EQ(expected, GetWebContentses());
}

IN_PROC_BROWSER_TEST_F(TabStripBrowsertest,
                       ShiftGroupRight_Failure_EdgeOfTabstrip) {
  AppendTab();
  AppendTab();

  tab_groups::TabGroupId group = AddTabToNewGroup(1);
  AddTabToExistingGroup(2, group);

  const auto contentses = GetWebContentses();
  tab_strip()->ShiftGroupRight(group);
  // No change expected.
  EXPECT_EQ(contentses, GetWebContentses());
}
