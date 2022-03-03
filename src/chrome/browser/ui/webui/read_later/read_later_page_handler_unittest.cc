// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/read_later/read_later_test_utils.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

constexpr char kTabUrl1[] = "http://foo/1";
constexpr char kTabUrl2[] = "http://foo/2";
constexpr char kTabUrl3[] = "http://foo/3";
constexpr char kTabUrl4[] = "http://foo/4";

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";

class MockPage : public read_later::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<read_later::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<read_later::mojom::Page> receiver_{this};

  MOCK_METHOD1(ItemsChanged,
               void(read_later::mojom::ReadLaterEntriesByStatusPtr));
  MOCK_METHOD1(CurrentPageActionButtonStateChanged,
               void(read_later::mojom::CurrentPageActionButtonState));
};

void ExpectNewReadLaterEntry(const read_later::mojom::ReadLaterEntry* entry,
                             const GURL& url,
                             const std::string& title) {
  EXPECT_EQ(title, entry->title);
  EXPECT_EQ(url.spec(), entry->url.spec());
}

class TestReadLaterPageHandler : public ReadLaterPageHandler {
 public:
  explicit TestReadLaterPageHandler(
      mojo::PendingRemote<read_later::mojom::Page> page,
      content::WebUI* test_web_ui)
      : ReadLaterPageHandler(
            mojo::PendingReceiver<read_later::mojom::PageHandler>(),
            std::move(page),
            nullptr,
            test_web_ui) {}
};

class TestReadLaterPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::SetLastActive(browser());

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());

    handler_ = std::make_unique<TestReadLaterPageHandler>(
        page_.BindAndGetRemote(), test_web_ui_.get());
    model_ =
        ReadingListModelFactory::GetForBrowserContext(browser()->profile());
    test::ReadingListLoadObserver(model_).Wait();

    AddTabWithTitle(browser(), GURL(kTabUrl1), kTabName1);
    AddTabWithTitle(browser(), GURL(kTabUrl2), kTabName2);
    AddTabWithTitle(browser(), GURL(kTabUrl3), kTabName3);
    AddTabWithTitle(browser(), GURL(kTabUrl4), kTabName4);

    model()->AddEntry(GURL(kTabUrl1), kTabName1,
                      reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
    model()->AddEntry(GURL(kTabUrl3), kTabName3,
                      reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
  }

  void TearDown() override {
    handler_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    browser()->tab_strip_model()->CloseAllTabs();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{ReadingListModelFactory::GetInstance(),
             ReadingListModelFactory::GetDefaultFactoryForTesting()}};
  }

  ReadingListModel* model() { return model_; }
  TestReadLaterPageHandler* handler() { return handler_.get(); }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    AddTab(browser, url);
    NavigateAndCommitActiveTabWithTitle(browser, url,
                                        base::ASCIIToUTF16(title));
  }

  void GetAndVerifyReadLaterEntries(
      size_t unread_size,
      size_t read_size,
      const std::vector<std::pair<GURL, std::string>>& expected_unread_data,
      const std::vector<std::pair<GURL, std::string>>& expected_read_data) {
    EXPECT_EQ(unread_size, expected_unread_data.size());
    read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback =
        base::BindLambdaForTesting(
            [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                    entries_by_status) {
              ASSERT_EQ(unread_size, entries_by_status->unread_entries.size());
              ASSERT_EQ(read_size, entries_by_status->read_entries.size());

              // Verify the entries appear in order of last added to first.
              for (size_t i = 0u; i < expected_unread_data.size(); i++) {
                auto* entry = entries_by_status->unread_entries[i].get();
                ExpectNewReadLaterEntry(entry, expected_unread_data[i].first,
                                        expected_unread_data[i].second);
              }

              // Verify the entries appear in order of last added to first.
              for (size_t i = 0u; i < expected_read_data.size(); i++) {
                auto* entry = entries_by_status->read_entries[i].get();
                ExpectNewReadLaterEntry(entry, expected_read_data[i].first,
                                        expected_read_data[i].second);
              }
            });
    handler()->GetReadLaterEntries(std::move(callback));
  }

  testing::StrictMock<MockPage> page_;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TestReadLaterPageHandler> handler_;
  raw_ptr<ReadingListModel> model_;
};

TEST_F(TestReadLaterPageHandlerTest, GetReadLaterEntries) {
  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);
  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 2u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3),
       std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest, OpenURLOnNTP) {
  // Open and navigate to NTP.
  AddTabWithTitle(browser(), GURL(chrome::kChromeUINewTabURL), "NTP");

  // Check that OpenURL from the NTP does not open a new tab.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);
  handler()->OpenURL(GURL(kTabUrl3), true, {});
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadLaterPageHandlerTest, OpenURLNotOnNTP) {
  // Check that OpenURL opens a new tab when not on the NTP.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 4);
  handler()->OpenURL(GURL(kTabUrl3), true, {});
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadLaterPageHandlerTest, UpdateReadStatus) {
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadLaterPageHandlerTest, RemoveEntry) {
  handler()->RemoveEntry(GURL(kTabUrl3));

  // Expect ItemsChanged to be called 5 times.
  // Four for the two AddEntry calls in SetUp().
  // Once for the RemoveEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest, UpdateAndRemoveEntry) {
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());
  handler()->OpenURL(GURL(kTabUrl3), true, {});
  handler()->RemoveEntry(GURL(kTabUrl3));
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());

  // Expect ItemsChanged to be called 6 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above.
  // Once for the RemoveEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(6);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest, PostBatchUpdate) {
  auto token = model()->BeginBatchUpdates();
  EXPECT_TRUE(model()->IsPerformingBatchUpdates());
  handler()->OpenURL(GURL(kTabUrl3), true, {});
  handler()->RemoveEntry(GURL(kTabUrl3));
  token.reset();
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the two updates above performed during a batch update.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest, NoUpdateWhenHidden) {
  // Set WebContents to be hidden.
  content::WebContents::CreateParams params =
      content::WebContents::CreateParams(profile());
  params.initially_hidden = true;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(params);
  handler()->set_web_contents_for_testing(web_contents.get());

  handler()->OpenURL(GURL(kTabUrl3), true, {});
  handler()->RemoveEntry(GURL(kTabUrl3));

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() and the two above calls to not trigger an ItemsChanged call because
  // the WebContents is not visible.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries. Calling GetReadLaterEntries will trigger an update.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest, OpenURLAndReadd) {
  // Check that OpenURL opens a new tab when not on the NTP.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 4);
  handler()->OpenURL(GURL(kTabUrl3), true, {});
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);
  model()->AddEntry(GURL(kTabUrl3), kTabName3,
                    reading_list::EntrySource::ADDED_VIA_CURRENT_APP);

  // Expect ItemsChanged to be called 6 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above, and twice for the AddEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(7);
  // Expect CurrentPageActionButtonStateChanged to be called once when the
  // current page is added while on that page.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(2);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 2u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3),
       std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadLaterPageHandlerTest,
       CurrentPageActionButtonStateChangedOnActiveTabChange) {
  handler()->SetActiveTabURL(GURL("http://google.com"));
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            read_later::mojom::CurrentPageActionButtonState::kAdd);
  handler()->SetActiveTabURL(GURL("google.com"));
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            read_later::mojom::CurrentPageActionButtonState::kDisabled);
  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called twice.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(2);
}

}  // namespace
