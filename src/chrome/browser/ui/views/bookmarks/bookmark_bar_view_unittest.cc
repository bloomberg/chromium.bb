// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class BookmarkBarViewBaseTest : public ChromeViewsTestBase {
 public:
  BookmarkBarViewBaseTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(
            &BookmarkBarViewBaseTest::CreateTemplateURLService));
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    Browser::CreateParams params(profile(), true);
    params.window = &browser_window_;
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    feature_list_.InitAndEnableFeature(features::kTabGroupsSave);
  }

  virtual BookmarkBarView* bookmark_bar_view() = 0;

  TestingProfile* profile() { return profile_.get(); }
  Browser* browser() { return browser_.get(); }

 protected:
  // Returns a string containing the label of each of the *visible* buttons on
  // the bookmark bar. Each label is separated by a space.
  std::string GetStringForVisibleButtons() {
    std::string result;
    for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount() &&
                       test_helper_->GetBookmarkButton(i)->GetVisible();
         ++i) {
      if (i != 0)
        result += " ";
      result +=
          base::UTF16ToASCII(test_helper_->GetBookmarkButton(i)->GetText());
    }
    return result;
  }

  // Continues sizing the bookmark bar until it has |count| buttons that are
  // visible.
  // NOTE: if the model has more than |count| buttons this results in
  // |count| + 1 buttons.
  void SizeUntilButtonsVisible(size_t count) {
    const int start_width = bookmark_bar_view()->width();
    const int height = bookmark_bar_view()->GetPreferredSize().height();
    for (size_t i = 0;
         i < 100 && (test_helper_->GetBookmarkButtonCount() < count ||
                     !test_helper_->GetBookmarkButton(count - 1)->GetVisible());
         ++i) {
      bookmark_bar_view()->SetBounds(0, 0, start_width + i * 10, height);
      bookmark_bar_view()->Layout();
    }
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  SavedTabGroupModel* stg_model() {
    return SavedTabGroupServiceFactory::GetForProfile(profile())->model();
  }

  void WaitForBookmarkModelToLoad() {
    bookmarks::test::WaitForBookmarkModelToLoad(model());
  }

  // Adds nodes to the bookmark bar node from |string|. See
  // bookmarks::test::AddNodesFromModelString() for details on |string|.
  void AddNodesToBookmarkBarFromModelString(const std::string& string) {
    bookmarks::test::AddNodesFromModelString(
        model(), model()->bookmark_bar_node(), string);
  }

  void AddGroupsToSTGModel() {
    size_t initial_button_count = test_helper_->GetTabGroupButtonCount();
    const std::u16string title_1 = u"First Group";
    const std::u16string title_2 = u"Second Group";
    const std::u16string title_3 = u"The Third Group";

    const tab_groups::TabGroupColorId& color_1 =
        tab_groups::TabGroupColorId::kGrey;
    const tab_groups::TabGroupColorId& color_2 =
        tab_groups::TabGroupColorId::kRed;
    const tab_groups::TabGroupColorId& color_3 =
        tab_groups::TabGroupColorId::kGreen;

    std::vector<SavedTabGroupTab> group_1_tabs = {
        CreateSavedTabGroupTab("A_Link", u"A Link")};
    std::vector<SavedTabGroupTab> group_2_tabs = {
        CreateSavedTabGroupTab("B_Link", u"B Link"),
        CreateSavedTabGroupTab("Not A Link", u"Not A Link")};
    std::vector<SavedTabGroupTab> group_3_tabs = {
        CreateSavedTabGroupTab("Mickey", u"Mickey"),
        CreateSavedTabGroupTab("Donald", u"Donald"),
        CreateSavedTabGroupTab("Goofy", u"Goofy")};

    SavedTabGroup group_1(id_1_, title_1, color_1, group_1_tabs);
    SavedTabGroup group_2(id_2_, title_2, color_2, group_2_tabs);
    SavedTabGroup group_3(id_3_, title_3, color_3, group_3_tabs);

    stg_model()->Add(
        CreateSavedTabGroup(id_1_, title_1, color_1, group_1_tabs));
    stg_model()->Add(
        CreateSavedTabGroup(id_2_, title_2, color_2, group_2_tabs));
    stg_model()->Add(
        CreateSavedTabGroup(id_3_, title_3, color_3, group_3_tabs));

    size_t current_button_count = test_helper_->GetTabGroupButtonCount();
    EXPECT_EQ(3u, current_button_count - initial_button_count);
    EXPECT_EQ(
        title_1,
        test_helper_->GetTabGroupButton(current_button_count - 3)->GetText());
    EXPECT_EQ(
        title_2,
        test_helper_->GetTabGroupButton(current_button_count - 2)->GetText());
    EXPECT_EQ(
        title_3,
        test_helper_->GetTabGroupButton(current_button_count - 1)->GetText());
  }

  std::vector<GURL> CreateGURL(std::vector<std::string> paths) {
    std::vector<GURL> gurls;
    gurls.reserve(paths.size());
    for (std::string path : paths)
      gurls.emplace_back(GURL(base_path_ + path));
    return gurls;
  }

  SavedTabGroupTab CreateSavedTabGroupTab(std::string url,
                                          std::u16string title) {
    return SavedTabGroupTab(GURL(base_path_ + url), title,
                            favicon::GetDefaultFavicon());
  }

  SavedTabGroup CreateSavedTabGroup(tab_groups::TabGroupId id,
                                    std::u16string group_title,
                                    tab_groups::TabGroupColorId color,
                                    std::vector<SavedTabGroupTab> group_tabs) {
    return SavedTabGroup(id, group_title, color, group_tabs);
  }

  // Creates the model, blocking until it loads, then creates the
  // BookmarkBarView.
  std::unique_ptr<BookmarkBarView> CreateBookmarkModelAndBookmarkBarView() {
    WaitForBookmarkModelToLoad();

    auto bookmark_bar_view =
        std::make_unique<BookmarkBarView>(browser(), nullptr);
    test_helper_ =
        std::make_unique<BookmarkBarViewTestHelper>(bookmark_bar_view.get());
    return bookmark_bar_view;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  TestBrowserWindow browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;
  std::string base_path_ = "file:///c:/tmp/";
  tab_groups::TabGroupId id_1_ = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId id_2_ = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId id_3_ = tab_groups::TabGroupId::GenerateNew();

 private:
  static std::unique_ptr<KeyedService> CreateTemplateURLService(
      content::BrowserContext* profile) {
    return std::make_unique<TemplateURLService>(
        static_cast<Profile*>(profile)->GetPrefs(),
        std::make_unique<SearchTermsData>(),
        nullptr /* KeywordWebDataService */,
        nullptr /* TemplateURLServiceClient */, base::RepeatingClosure());
  }
};

class BookmarkBarViewTest : public BookmarkBarViewBaseTest {
 public:
  BookmarkBarViewTest() = default;
  BookmarkBarViewTest(const BookmarkBarViewTest&) = delete;
  BookmarkBarViewTest& operator=(const BookmarkBarViewTest&) = delete;
  ~BookmarkBarViewTest() override = default;

  // BookmarkBarViewBaseTest
  void SetUp() override {
    BookmarkBarViewBaseTest::SetUp();

    bookmark_bar_view_ = CreateBookmarkModelAndBookmarkBarView();
  }

  void TearDown() override {
    BookmarkBarViewBaseTest::TearDown();

    bookmark_bar_view_.reset();
  }

  BookmarkBarView* bookmark_bar_view() override {
    return bookmark_bar_view_.get();
  }

 private:
  std::unique_ptr<BookmarkBarView> bookmark_bar_view_;
};

class BookmarkBarViewInWidgetTest : public BookmarkBarViewBaseTest {
 public:
  BookmarkBarViewInWidgetTest() = default;
  BookmarkBarViewInWidgetTest(const BookmarkBarViewInWidgetTest&) = delete;
  BookmarkBarViewInWidgetTest& operator=(const BookmarkBarViewInWidgetTest&) =
      delete;
  ~BookmarkBarViewInWidgetTest() override = default;

  // BookmarkBarViewBaseTest
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    BookmarkBarViewBaseTest::SetUp();

    widget_ = CreateTestWidget();
    bookmark_bar_view_ =
        widget_->SetContentsView(CreateBookmarkModelAndBookmarkBarView());
  }

  void TearDown() override {
    widget_.reset();

    BookmarkBarViewBaseTest::TearDown();
  }

  BookmarkBarView* bookmark_bar_view() override { return bookmark_bar_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<BookmarkBarView> bookmark_bar_view_ = nullptr;
};

// Verify that adding a single tab group button adds only 1 button and it is
// appended to the end of the list.
TEST_F(BookmarkBarViewTest, AddAdditionalTabGroupButton) {
  // Adds 3 buttons to our model.
  AddGroupsToSTGModel();

  // Create a new SavedTabGroup and add it to our model.
  tab_groups::TabGroupId group_id(tab_groups::TabGroupId::GenerateNew());
  const std::u16string group_title = u"Additional Group";
  const tab_groups::TabGroupColorId& group_color =
      tab_groups::TabGroupColorId::kBlue;

  std::vector<SavedTabGroupTab> group_tabs = {
      CreateSavedTabGroupTab("Additional_Link", u"Additional Link")};
  stg_model()->Add(
      CreateSavedTabGroup(group_id, group_title, group_color, group_tabs));

  // Verify we have 4 buttons and the title of the group is the same.
  EXPECT_EQ(4u, test_helper_->GetTabGroupButtonCount());
  EXPECT_EQ(group_title, test_helper_->GetTabGroupButton(3)->GetText());
}

// Verify that removing a tab group from the model removes the correct button.
TEST_F(BookmarkBarViewTest, RemoveTabGroupButton) {
  // Adds 3 buttons to our model.
  AddGroupsToSTGModel();

  // Removes the second tab group button.
  stg_model()->Remove(id_2_);

  // Verify we have 2 buttons and do not have the title of the second tab group.
  EXPECT_EQ(2u, test_helper_->GetTabGroupButtonCount());
  EXPECT_EQ(u"First Group", test_helper_->GetTabGroupButton(0)->GetText());
  EXPECT_EQ(u"The Third Group", test_helper_->GetTabGroupButton(1)->GetText());
}

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, AppsShortcutVisibility) {
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the Apps shortcut is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, true);
  if (chrome::IsAppsShortcutEnabled(browser()->profile())) {
    EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());
  } else {
    EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
  }

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
}

// Various assertions around visibility of the overflow_button.
TEST_F(BookmarkBarViewTest, OverflowVisibility) {
  EXPECT_FALSE(test_helper_->overflow_button()->GetVisible());

  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());
  const int width_for_one = bookmark_bar_view()->bounds().width();
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  EXPECT_FALSE(test_helper_->overflow_button()->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, width_for_one,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_TRUE(test_helper_->overflow_button()->GetVisible());
}

// Verifies buttons get added correctly when BookmarkBarView is created after
// the model and the model has nodes.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAddedAfterModelHasNodes) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  // Ensure buttons were added in the correct place.
  auto button_iter =
      bookmark_bar_view()->FindChild(test_helper_->managed_bookmarks_button());
  for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    ++button_iter;
    ASSERT_NE(bookmark_bar_view()->children().cend(), button_iter);
    EXPECT_EQ(test_helper_->GetBookmarkButton(i), *button_iter);
  }
}

// Verifies buttons are added as the model and size change.
TEST_F(BookmarkBarViewTest, ButtonsDynamicallyAdded) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Go really big, which should force all buttons to be added.
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());
  // Ensure buttons were added in the correct place.
  auto button_iter =
      bookmark_bar_view()->FindChild(test_helper_->managed_bookmarks_button());
  for (size_t i = 0; i < test_helper_->GetBookmarkButtonCount(); ++i) {
    ++button_iter;
    ASSERT_NE(bookmark_bar_view()->children().cend(), button_iter);
    EXPECT_EQ(test_helper_->GetBookmarkButton(i), *button_iter);
  }
}

TEST_F(BookmarkBarViewTest, AddNodesWhenBarAlreadySized) {
  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  bookmark_bar_view()->Layout();
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
}

// Various assertions for removing nodes.
TEST_F(BookmarkBarViewTest, RemoveNode) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());
  SizeUntilButtonsVisible(1);
  EXPECT_EQ(2u, test_helper_->GetBookmarkButtonCount());

  // Remove the 2nd node, should still only have 1 visible.
  model()->Remove(bookmark_bar_node->children()[1].get());
  EXPECT_EQ("a", GetStringForVisibleButtons());

  // Remove the first node, should force a new button (for the 'c' node).
  model()->Remove(bookmark_bar_node->children()[0].get());
  ASSERT_EQ("c", GetStringForVisibleButtons());
}

// Assertions for moving a node on the bookmark bar.
TEST_F(BookmarkBarViewTest, MoveNode) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Move 'c' first resulting in 'c a b d e f'.
  model()->Move(bookmark_bar_node->children()[2].get(), bookmark_bar_node, 0);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("c", GetStringForVisibleButtons());

  // Move 'f' first, resulting in 'f c a b d e'.
  model()->Move(bookmark_bar_node->children()[5].get(), bookmark_bar_node, 0);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("f c", GetStringForVisibleButtons());

  // Move 'f' to the end, resulting in 'c a b d e f'.
  model()->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 6);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("c a", GetStringForVisibleButtons());

  // Move 'c' after 'a', resulting in 'a c b d e f'.
  model()->Move(bookmark_bar_node->children()[0].get(), bookmark_bar_node, 2);
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a c", GetStringForVisibleButtons());
}

// Assertions for changing the title of a node.
TEST_F(BookmarkBarViewTest, ChangeTitle) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  model()->SetTitle(bookmark_bar_node->children()[0].get(), u"a1");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  // Make enough room for 1 node.
  SizeUntilButtonsVisible(1);
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[1].get(), u"b1");
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[5].get(), u"f1");
  EXPECT_EQ("a1", GetStringForVisibleButtons());

  model()->SetTitle(bookmark_bar_node->children()[3].get(), u"d1");

  // Make the second button visible, changes the title of the first to something
  // really long and make sure the second button hides.
  SizeUntilButtonsVisible(2);
  EXPECT_EQ("a1 b1", GetStringForVisibleButtons());
  model()->SetTitle(bookmark_bar_node->children()[0].get(),
                    u"a_really_long_title");
  EXPECT_LE(1u, test_helper_->GetBookmarkButtonCount());

  // Change the title back and make sure the 2nd button is visible again. Don't
  // use GetStringForVisibleButtons() here as more buttons may have been
  // created.
  model()->SetTitle(bookmark_bar_node->children()[0].get(), u"a1");
  ASSERT_LE(2u, test_helper_->GetBookmarkButtonCount());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(0)->GetVisible());
  EXPECT_TRUE(test_helper_->GetBookmarkButton(1)->GetVisible());

  bookmark_bar_view()->SetBounds(0, 0, 5000,
                                 bookmark_bar_view()->bounds().height());
  bookmark_bar_view()->Layout();
  EXPECT_EQ("a1 b1 c d1 e f1", GetStringForVisibleButtons());
}

TEST_F(BookmarkBarViewTest, DropCallbackTest) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  SizeUntilButtonsVisible(7);
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  gfx::Point bar_loc;
  views::View::ConvertPointToScreen(bookmark_bar_view(), &bar_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(bar_loc),
                                   gfx::PointF(bar_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  EXPECT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  auto cb = bookmark_bar_view()->GetDropCallback(target_event);
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());

  ui::mojom::DragOperation output_drag_op;
  std::move(cb).Run(target_event, output_drag_op);
  EXPECT_EQ("z a b c d e f", GetStringForVisibleButtons());
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kCopy);
}

TEST_F(BookmarkBarViewTest, MutateModelDuringDrag) {
  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  SizeUntilButtonsVisible(7);
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  gfx::Point drop_loc;
  views::View::ConvertPointToScreen(test_helper_->GetBookmarkButton(5),
                                    &drop_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(drop_loc),
                                   gfx::PointF(drop_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  ASSERT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  EXPECT_NE(-1, test_helper_->GetDropLocationModelIndexForTesting());
  model()->Remove(model()->bookmark_bar_node()->children()[4].get());
  EXPECT_EQ(-1, test_helper_->GetDropLocationModelIndexForTesting());
}

TEST_F(BookmarkBarViewTest, DropCallback_InvalidatePtrTest) {
  SizeUntilButtonsVisible(7);
  EXPECT_EQ(0u, test_helper_->GetBookmarkButtonCount());

  gfx::Point bar_loc;
  views::View::ConvertPointToScreen(bookmark_bar_view(), &bar_loc);
  ui::OSExchangeData drop_data;
  drop_data.SetURL(GURL("http://www.chromium.org/"), std::u16string(u"z"));
  ui::DropTargetEvent target_event(drop_data, gfx::PointF(bar_loc),
                                   gfx::PointF(bar_loc),
                                   ui::DragDropTypes::DRAG_COPY);
  EXPECT_TRUE(bookmark_bar_view()->CanDrop(drop_data));
  bookmark_bar_view()->OnDragUpdated(target_event);
  auto cb = bookmark_bar_view()->GetDropCallback(target_event);

  AddNodesToBookmarkBarFromModelString("a b c d e f ");
  EXPECT_EQ(6u, test_helper_->GetBookmarkButtonCount());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(target_event, output_drag_op);
  EXPECT_EQ("a b c d e f", GetStringForVisibleButtons());
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kNone);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  // By default, the pref is not managed and the apps shortcut is not shown.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());

  // Shows the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->GetVisible());

  // And try hiding it via policy too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        std::make_unique<base::Value>(false));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->GetVisible());
}
#endif

TEST_F(BookmarkBarViewInWidgetTest, UpdateTooltipText) {
  widget()->Show();

  bookmarks::test::AddNodesFromModelString(model(),
                                           model()->bookmark_bar_node(), "a b");
  SizeUntilButtonsVisible(1);
  ASSERT_EQ(1u, test_helper_->GetBookmarkButtonCount());

  views::LabelButton* button = test_helper_->GetBookmarkButton(0);
  ASSERT_TRUE(button);
  gfx::Point p;
  EXPECT_EQ(u"a\na.com", button->GetTooltipText(p));
  button->SetText(u"new title");
  EXPECT_EQ(u"new title\na.com", button->GetTooltipText(p));
}

}  // namespace
