// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/test/app_list_test_model.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_switcher_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/app_list/views/test/apps_grid_view_test_api.h"
#include "ui/app_list/views/tile_item_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

namespace app_list {
namespace test {

namespace {

enum TestType {
  TEST_TYPE_START = 0,
  NORMAL = TEST_TYPE_START,
  LANDSCAPE,
  EXPERIMENTAL,
  TEST_TYPE_END,
};

bool IsViewAtOrigin(views::View* view) {
  return view->bounds().origin().IsOrigin();
}

size_t GetVisibleTileItemViews(const std::vector<TileItemView*>& tiles) {
  size_t count = 0;
  for (std::vector<TileItemView*>::const_iterator it = tiles.begin();
       it != tiles.end();
       ++it) {
    if ((*it)->visible())
      count++;
  }
  return count;
}

// Choose a set that is 3 regular app list pages and 2 landscape app list pages.
const int kInitialItems = 34;

class TestTileSearchResult : public SearchResult {
 public:
  TestTileSearchResult() { set_display_type(DISPLAY_TILE); }
  virtual ~TestTileSearchResult() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTileSearchResult);
};

// Allows the same tests to run with different contexts: either an Ash-style
// root window or a desktop window tree host.
class AppListViewTestContext {
 public:
  AppListViewTestContext(int test_type, gfx::NativeView parent);
  ~AppListViewTestContext();

  // Test displaying the app list and performs a standard set of checks on its
  // top level views. Then closes the window.
  void RunDisplayTest();

  // Hides and reshows the app list with a folder open, expecting the main grid
  // view to be shown.
  void RunReshowWithOpenFolderTest();

  // Tests displaying of the experimental app list and shows the start page.
  void RunStartPageTest();

  // Tests switching rapidly between multiple pages of the launcher.
  void RunPageSwitchingAnimationTest();

  // Tests changing the App List profile.
  void RunProfileChangeTest();

  // Tests displaying of the search results.
  void RunSearchResultsTest();

  // A standard set of checks on a view, e.g., ensuring it is drawn and visible.
  static void CheckView(views::View* subview);

  // Invoked when the Widget is closing, and the view it contains is about to
  // be torn down. This only occurs in a run loop and will be used as a signal
  // to quit.
  void NativeWidgetClosing() {
    view_ = NULL;
    run_loop_->Quit();
  }

  // Whether the experimental "landscape" app launcher UI is being tested.
  bool is_landscape() const {
    return test_type_ == LANDSCAPE || test_type_ == EXPERIMENTAL;
  }

 private:
  // Switches the active launcher page in the contents view and lays out to
  // ensure all launcher pages are in the correct position.
  void ShowContentsViewPageAndVerify(int index);

  // Shows the app list and waits until a paint occurs.
  void Show();

  // Closes the app list. This sets |view_| to NULL.
  void Close();

  // Gets the PaginationModel owned by |view_|.
  PaginationModel* GetPaginationModel();

  const TestType test_type_;
  scoped_ptr<base::RunLoop> run_loop_;
  app_list::AppListView* view_;  // Owned by native widget.
  app_list::test::AppListTestViewDelegate* delegate_;  // Owned by |view_|;

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestContext);
};

// Extend the regular AppListTestViewDelegate to communicate back to the test
// context. Note the test context doesn't simply inherit this, because the
// delegate is owned by the view.
class UnitTestViewDelegate : public app_list::test::AppListTestViewDelegate {
 public:
  UnitTestViewDelegate(AppListViewTestContext* parent) : parent_(parent) {}

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ShouldCenterWindow() const OVERRIDE {
    return app_list::switches::IsCenteredAppListEnabled();
  }

  // Overridden from app_list::test::AppListTestViewDelegate:
  virtual void ViewClosing() OVERRIDE { parent_->NativeWidgetClosing(); }

 private:
  AppListViewTestContext* parent_;

  DISALLOW_COPY_AND_ASSIGN(UnitTestViewDelegate);
};

AppListViewTestContext::AppListViewTestContext(int test_type,
                                               gfx::NativeView parent)
    : test_type_(static_cast<TestType>(test_type)) {
  switch (test_type_) {
    case NORMAL:
      break;
    case LANDSCAPE:
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableCenteredAppList);
      break;
    case EXPERIMENTAL:
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableExperimentalAppList);
      break;
    default:
      NOTREACHED();
      break;
  }

  delegate_ = new UnitTestViewDelegate(this);
  view_ = new app_list::AppListView(delegate_);

  // Initialize centered around a point that ensures the window is wholly shown.
  view_->InitAsBubbleAtFixedLocation(parent,
                                     0,
                                     gfx::Point(300, 300),
                                     views::BubbleBorder::FLOAT,
                                     false /* border_accepts_events */);
}

AppListViewTestContext::~AppListViewTestContext() {
  // The view observes the PaginationModel which is about to get destroyed, so
  // if the view is not already deleted by the time this destructor is called,
  // there will be problems.
  EXPECT_FALSE(view_);
}

// static
void AppListViewTestContext::CheckView(views::View* subview) {
  ASSERT_TRUE(subview);
  EXPECT_TRUE(subview->parent());
  EXPECT_TRUE(subview->visible());
  EXPECT_TRUE(subview->IsDrawn());
}

void AppListViewTestContext::ShowContentsViewPageAndVerify(int index) {
  ContentsView* contents_view = view_->app_list_main_view()->contents_view();
  contents_view->SetActivePage(index);
  contents_view->Layout();
  for (int i = 0; i < contents_view->NumLauncherPages(); ++i) {
    EXPECT_EQ(i == index, IsViewAtOrigin(contents_view->GetPageView(i)));
  }
}

void AppListViewTestContext::Show() {
  view_->GetWidget()->Show();
  run_loop_.reset(new base::RunLoop);
  view_->SetNextPaintCallback(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_TRUE(view_->GetWidget()->IsVisible());
}

void AppListViewTestContext::Close() {
  view_->GetWidget()->Close();
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();

  // |view_| should have been deleted and set to NULL via ViewClosing().
  EXPECT_FALSE(view_);
}

PaginationModel* AppListViewTestContext::GetPaginationModel() {
  return view_->GetAppsPaginationModel();
}

void AppListViewTestContext::RunDisplayTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();
  if (is_landscape())
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
  else
    EXPECT_EQ(3, GetPaginationModel()->total_pages());
  EXPECT_EQ(0, GetPaginationModel()->selected_page());

  // Checks on the main view.
  AppListMainView* main_view = view_->app_list_main_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

  EXPECT_TRUE(main_view->contents_view()->IsNamedPageActive(
      test_type_ == EXPERIMENTAL ? ContentsView::NAMED_PAGE_START
                                 : ContentsView::NAMED_PAGE_APPS));

  Close();
}

void AppListViewTestContext::RunReshowWithOpenFolderTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());

  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(kInitialItems);
  const std::string folder_id =
      model->MergeItems(model->top_level_item_list()->item_at(0)->id(),
                        model->top_level_item_list()->item_at(1)->id());

  AppListFolderItem* folder_item = model->FindFolderItem(folder_id);
  EXPECT_TRUE(folder_item);

  Show();

  // The main grid view should be showing initially.
  AppListMainView* main_view = view_->app_list_main_view();
  AppsContainerView* container_view =
      main_view->contents_view()->apps_container_view();
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->apps_grid_view()));
  EXPECT_FALSE(container_view->app_list_folder_view()->visible());

  AppsGridViewTestApi test_api(container_view->apps_grid_view());
  test_api.PressItemAt(0);

  // After pressing the folder item, the folder view should be showing.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->app_list_folder_view()));
  EXPECT_FALSE(container_view->apps_grid_view()->visible());

  view_->GetWidget()->Hide();
  EXPECT_FALSE(view_->GetWidget()->IsVisible());

  Show();

  // The main grid view should be showing after a reshow.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(container_view->apps_grid_view()));
  EXPECT_FALSE(container_view->app_list_folder_view()->visible());

  Close();
}

void AppListViewTestContext::RunStartPageTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(3);

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  StartPageView* start_page_view =
      main_view->contents_view()->start_page_view();
  // Checks on the main view.
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
  EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));
  if (test_type_ == EXPERIMENTAL) {
    EXPECT_NO_FATAL_FAILURE(CheckView(start_page_view));

    // Show the start page view.
    ContentsView* contents_view = main_view->contents_view();
    ShowContentsViewPageAndVerify(contents_view->GetPageIndexForNamedPage(
        ContentsView::NAMED_PAGE_START));
    EXPECT_FALSE(main_view->search_box_view()->visible());

    gfx::Size view_size(view_->GetPreferredSize());
    ShowContentsViewPageAndVerify(
        contents_view->GetPageIndexForNamedPage(ContentsView::NAMED_PAGE_APPS));
    EXPECT_TRUE(main_view->search_box_view()->visible());

    // Hiding and showing the search box should not affect the app list's
    // preferred size. This is a regression test for http://crbug.com/386912.
    EXPECT_EQ(view_size.ToString(), view_->GetPreferredSize().ToString());

    // Check tiles hide and show on deletion and addition.
    model->results()->Add(new TestTileSearchResult());
    start_page_view->UpdateForTesting();
    EXPECT_EQ(1u, GetVisibleTileItemViews(start_page_view->tile_views()));
    model->results()->DeleteAll();
    start_page_view->UpdateForTesting();
    EXPECT_EQ(0u, GetVisibleTileItemViews(start_page_view->tile_views()));
  } else {
    EXPECT_EQ(NULL, start_page_view);
  }

  Close();
}

void AppListViewTestContext::RunPageSwitchingAnimationTest() {
  if (test_type_ == EXPERIMENTAL) {
    Show();

    AppListMainView* main_view = view_->app_list_main_view();
    // Checks on the main view.
    EXPECT_NO_FATAL_FAILURE(CheckView(main_view));
    EXPECT_NO_FATAL_FAILURE(CheckView(main_view->contents_view()));

    ContentsView* contents_view = main_view->contents_view();
    // Pad the ContentsView with blank pages so we have at least 3 views.
    while (contents_view->NumLauncherPages() < 3)
      contents_view->AddBlankPageForTesting();

    contents_view->SetActivePage(0);
    contents_view->Layout();
    EXPECT_TRUE(IsViewAtOrigin(contents_view->GetPageView(0)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(1)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(2)));

    // Change pages. View should not have moved without Layout().
    contents_view->SetActivePage(1);
    EXPECT_TRUE(IsViewAtOrigin(contents_view->GetPageView(0)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(1)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(2)));

    // Change to a third page. This queues up the second animation behind the
    // first.
    contents_view->SetActivePage(2);
    EXPECT_TRUE(IsViewAtOrigin(contents_view->GetPageView(0)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(1)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(2)));

    // Call Layout(). Should jump to the third page.
    contents_view->Layout();
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(0)));
    EXPECT_FALSE(IsViewAtOrigin(contents_view->GetPageView(1)));
    EXPECT_TRUE(IsViewAtOrigin(contents_view->GetPageView(2)));
  }

  Close();
}

void AppListViewTestContext::RunProfileChangeTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  delegate_->GetTestModel()->PopulateApps(kInitialItems);

  Show();

  if (is_landscape())
    EXPECT_EQ(2, GetPaginationModel()->total_pages());
  else
    EXPECT_EQ(3, GetPaginationModel()->total_pages());

  // Change the profile. The original model needs to be kept alive for
  // observers to unregister themselves.
  scoped_ptr<AppListTestModel> original_test_model(
      delegate_->ReleaseTestModel());
  delegate_->set_next_profile_app_count(1);

  // The original ContentsView is destroyed here.
  view_->SetProfileByPath(base::FilePath());
  EXPECT_EQ(1, GetPaginationModel()->total_pages());

  StartPageView* start_page_view =
      view_->app_list_main_view()->contents_view()->start_page_view();
  ContentsSwitcherView* contents_switcher_view =
      view_->app_list_main_view()->contents_switcher_view();
  if (test_type_ == EXPERIMENTAL) {
    EXPECT_NO_FATAL_FAILURE(CheckView(contents_switcher_view));
    EXPECT_EQ(view_->app_list_main_view()->contents_view(),
              contents_switcher_view->contents_view());
    EXPECT_NO_FATAL_FAILURE(CheckView(start_page_view));
  } else {
    EXPECT_EQ(NULL, contents_switcher_view);
    EXPECT_EQ(NULL, start_page_view);
  }

  // New model updates should be processed by the start page view.
  delegate_->GetTestModel()->results()->Add(new TestTileSearchResult());
  if (test_type_ == EXPERIMENTAL) {
    start_page_view->UpdateForTesting();
    EXPECT_EQ(1u, GetVisibleTileItemViews(start_page_view->tile_views()));
  }

  // Old model updates should be ignored.
  original_test_model->results()->Add(new TestTileSearchResult());
  original_test_model->results()->Add(new TestTileSearchResult());
  if (test_type_ == EXPERIMENTAL) {
    start_page_view->UpdateForTesting();
    EXPECT_EQ(1u, GetVisibleTileItemViews(start_page_view->tile_views()));
  }

  Close();
}

void AppListViewTestContext::RunSearchResultsTest() {
  EXPECT_FALSE(view_->GetWidget()->IsVisible());
  EXPECT_EQ(-1, GetPaginationModel()->total_pages());
  AppListTestModel* model = delegate_->GetTestModel();
  model->PopulateApps(3);

  Show();

  AppListMainView* main_view = view_->app_list_main_view();
  ContentsView* contents_view = main_view->contents_view();
  ShowContentsViewPageAndVerify(
      contents_view->GetPageIndexForNamedPage(ContentsView::NAMED_PAGE_APPS));
  EXPECT_TRUE(main_view->search_box_view()->visible());

  // Show the search results.
  contents_view->ShowSearchResults(true);
  contents_view->Layout();
  EXPECT_TRUE(contents_view->IsShowingSearchResults());
  EXPECT_TRUE(main_view->search_box_view()->visible());

  if (test_type_ == EXPERIMENTAL) {
    EXPECT_TRUE(
        contents_view->IsNamedPageActive(ContentsView::NAMED_PAGE_START));
    EXPECT_TRUE(IsViewAtOrigin(contents_view->start_page_view()));
  } else {
    EXPECT_TRUE(contents_view->IsNamedPageActive(
        ContentsView::NAMED_PAGE_SEARCH_RESULTS));
    EXPECT_TRUE(IsViewAtOrigin(contents_view->search_results_view()));
  }

  // Hide the search results.
  contents_view->ShowSearchResults(false);
  contents_view->Layout();
  EXPECT_FALSE(contents_view->IsShowingSearchResults());

  // Check that we return to the page that we were on before the search.
  EXPECT_TRUE(contents_view->IsNamedPageActive(ContentsView::NAMED_PAGE_APPS));
  EXPECT_TRUE(IsViewAtOrigin(contents_view->apps_container_view()));
  EXPECT_TRUE(main_view->search_box_view()->visible());

  if (test_type_ == EXPERIMENTAL) {
    ShowContentsViewPageAndVerify(contents_view->GetPageIndexForNamedPage(
        ContentsView::NAMED_PAGE_START));

    // Check that typing into the dummy search box triggers the search page.
    base::string16 search_text = base::UTF8ToUTF16("test");
    SearchBoxView* dummy_search_box =
        contents_view->start_page_view()->dummy_search_box_view();
    EXPECT_TRUE(dummy_search_box->IsDrawn());
    dummy_search_box->search_box()->InsertText(search_text);
    contents_view->Layout();
    // Check that the current search is using |search_text|.
    EXPECT_EQ(search_text, delegate_->GetTestModel()->search_box()->text());
    EXPECT_TRUE(contents_view->IsShowingSearchResults());
    EXPECT_FALSE(dummy_search_box->IsDrawn());
    EXPECT_TRUE(main_view->search_box_view()->visible());
    EXPECT_EQ(search_text, main_view->search_box_view()->search_box()->text());
    EXPECT_TRUE(
        contents_view->IsNamedPageActive(ContentsView::NAMED_PAGE_START));
    EXPECT_TRUE(IsViewAtOrigin(contents_view->start_page_view()));

    // Check that typing into the real search box triggers the search page.
    ShowContentsViewPageAndVerify(
        contents_view->GetPageIndexForNamedPage(ContentsView::NAMED_PAGE_APPS));
    EXPECT_TRUE(IsViewAtOrigin(contents_view->apps_container_view()));

    base::string16 new_search_text = base::UTF8ToUTF16("apple");
    main_view->search_box_view()->search_box()->SetText(base::string16());
    main_view->search_box_view()->search_box()->InsertText(new_search_text);
    // Check that the current search is using |search_text|.
    EXPECT_EQ(new_search_text, delegate_->GetTestModel()->search_box()->text());
    EXPECT_EQ(new_search_text,
              main_view->search_box_view()->search_box()->text());
    EXPECT_TRUE(contents_view->IsShowingSearchResults());
    EXPECT_FALSE(dummy_search_box->IsDrawn());
    EXPECT_TRUE(main_view->search_box_view()->visible());
    EXPECT_TRUE(dummy_search_box->search_box()->text().empty());

    // Check that the dummy search box is clear when reshowing the start page.
    ShowContentsViewPageAndVerify(
        contents_view->GetPageIndexForNamedPage(ContentsView::NAMED_PAGE_APPS));
    ShowContentsViewPageAndVerify(contents_view->GetPageIndexForNamedPage(
        ContentsView::NAMED_PAGE_START));
    EXPECT_TRUE(dummy_search_box->IsDrawn());
    EXPECT_TRUE(dummy_search_box->search_box()->text().empty());
  }

  Close();
}

class AppListViewTestAura : public views::ViewsTestBase,
                            public ::testing::WithParamInterface<int> {
 public:
  AppListViewTestAura() {}
  virtual ~AppListViewTestAura() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();

    // On Ash (only) the app list is placed into an aura::Window "container",
    // which is also used to determine the context. In tests, use the ash root
    // window as the parent. This only works on aura where the root window is a
    // NativeView as well as a NativeWindow.
    gfx::NativeView container = NULL;
#if defined(USE_AURA)
    container = GetContext();
#endif

    test_context_.reset(new AppListViewTestContext(GetParam(), container));
  }

  virtual void TearDown() OVERRIDE {
    test_context_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  scoped_ptr<AppListViewTestContext> test_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewTestAura);
};

class AppListViewTestDesktop : public views::ViewsTestBase,
                               public ::testing::WithParamInterface<int> {
 public:
  AppListViewTestDesktop() {}
  virtual ~AppListViewTestDesktop() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    set_views_delegate(new AppListViewTestViewsDelegate(this));
    views::ViewsTestBase::SetUp();
    test_context_.reset(new AppListViewTestContext(GetParam(), NULL));
  }

  virtual void TearDown() OVERRIDE {
    test_context_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  scoped_ptr<AppListViewTestContext> test_context_;

 private:
  class AppListViewTestViewsDelegate : public views::TestViewsDelegate {
   public:
    AppListViewTestViewsDelegate(AppListViewTestDesktop* parent)
        : parent_(parent) {}

    // Overridden from views::ViewsDelegate:
    virtual void OnBeforeWidgetInit(
        views::Widget::InitParams* params,
        views::internal::NativeWidgetDelegate* delegate) OVERRIDE;

   private:
    AppListViewTestDesktop* parent_;

    DISALLOW_COPY_AND_ASSIGN(AppListViewTestViewsDelegate);
  };

  DISALLOW_COPY_AND_ASSIGN(AppListViewTestDesktop);
};

void AppListViewTestDesktop::AppListViewTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
// Mimic the logic in ChromeViewsDelegate::OnBeforeWidgetInit(). Except, for
// ChromeOS, use the root window from the AuraTestHelper rather than depending
// on ash::Shell:GetPrimaryRootWindow(). Also assume non-ChromeOS is never the
// Ash desktop, as that is covered by AppListViewTestAura.
#if defined(OS_CHROMEOS)
  if (!params->parent && !params->context)
    params->context = parent_->GetContext();
#elif defined(USE_AURA)
  if (params->parent == NULL && params->context == NULL && !params->child)
    params->native_widget = new views::DesktopNativeWidgetAura(delegate);
#endif
}

}  // namespace

// Tests showing the app list with basic test model in an ash-style root window.
TEST_P(AppListViewTestAura, Display) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunDisplayTest());
}

// Tests showing the app list on the desktop. Note on ChromeOS, this will still
// use the regular root window.
TEST_P(AppListViewTestDesktop, Display) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunDisplayTest());
}

// Tests that the main grid view is shown after hiding and reshowing the app
// list with a folder view open. This is a regression test for crbug.com/357058.
TEST_P(AppListViewTestAura, ReshowWithOpenFolder) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunReshowWithOpenFolderTest());
}

TEST_P(AppListViewTestDesktop, ReshowWithOpenFolder) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunReshowWithOpenFolderTest());
}

// Tests that the start page view operates correctly.
TEST_P(AppListViewTestAura, StartPageTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunStartPageTest());
}

TEST_P(AppListViewTestDesktop, StartPageTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunStartPageTest());
}

// Tests that the start page view operates correctly.
TEST_P(AppListViewTestAura, PageSwitchingAnimationTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunPageSwitchingAnimationTest());
}

TEST_P(AppListViewTestDesktop, PageSwitchingAnimationTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunPageSwitchingAnimationTest());
}

// Tests that the profile changes operate correctly.
TEST_P(AppListViewTestAura, ProfileChangeTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunProfileChangeTest());
}

TEST_P(AppListViewTestDesktop, ProfileChangeTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunProfileChangeTest());
}

// Tests that the correct views are displayed for showing search results.
TEST_P(AppListViewTestAura, SearchResultsTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunSearchResultsTest());
}

TEST_P(AppListViewTestDesktop, SearchResultsTest) {
  EXPECT_NO_FATAL_FAILURE(test_context_->RunSearchResultsTest());
}

INSTANTIATE_TEST_CASE_P(AppListViewTestAuraInstance,
                        AppListViewTestAura,
                        ::testing::Range<int>(TEST_TYPE_START, TEST_TYPE_END));

INSTANTIATE_TEST_CASE_P(AppListViewTestDesktopInstance,
                        AppListViewTestDesktop,
                        ::testing::Range<int>(TEST_TYPE_START, TEST_TYPE_END));

}  // namespace test
}  // namespace app_list
