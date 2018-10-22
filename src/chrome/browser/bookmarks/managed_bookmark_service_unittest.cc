// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmark_service.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using testing::Mock;
using testing::_;

class ManagedBookmarkServiceTest : public testing::Test {
 public:
  ManagedBookmarkServiceTest() : managed_(NULL), model_(NULL) {}
  ~ManagedBookmarkServiceTest() override {}

  void SetUp() override {
    prefs_ = profile_.GetTestingPrefService();
    ASSERT_FALSE(prefs_->HasPrefPath(bookmarks::prefs::kManagedBookmarks));

    // TODO(crbug.com/697817): Convert SetManagedPrefs to take a unique_ptr.
    prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                           CreateTestTree());
    ResetModel();

    // The managed node always exists.
    ASSERT_TRUE(managed_->managed_node());
    ASSERT_TRUE(managed_->managed_node()->parent() == model_->root_node());
    EXPECT_NE(-1, model_->root_node()->GetIndexOf(managed_->managed_node()));
  }

  void TearDown() override { model_->RemoveObserver(&observer_); }

  void ResetModel() {
    profile_.CreateBookmarkModel(false);
    model_ = BookmarkModelFactory::GetForBrowserContext(&profile_);
    bookmarks::test::WaitForBookmarkModelToLoad(model_);
    model_->AddObserver(&observer_);
    managed_ = ManagedBookmarkServiceFactory::GetForProfile(&profile_);
    DCHECK(managed_);
  }

  static std::unique_ptr<base::DictionaryValue> CreateBookmark(
      const std::string& title,
      const std::string& url) {
    EXPECT_TRUE(GURL(url).is_valid());
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("name", title);
    dict->SetString("url", GURL(url).spec());
    return dict;
  }

  static std::unique_ptr<base::DictionaryValue> CreateFolder(
      const std::string& title,
      std::unique_ptr<base::ListValue> children) {
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("name", title);
    dict->Set("children", std::move(children));
    return dict;
  }

  static std::unique_ptr<base::ListValue> CreateTestTree() {
    auto folder = std::make_unique<base::ListValue>();
    folder->Append(CreateFolder("Empty", std::make_unique<base::ListValue>()));
    folder->Append(CreateBookmark("Youtube", "http://youtube.com/"));

    auto list = std::make_unique<base::ListValue>();
    list->Append(CreateBookmark("Google", "http://google.com/"));
    list->Append(CreateFolder("Folder", std::move(folder)));

    return list;
  }

  static std::unique_ptr<base::DictionaryValue> CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }

  static bool NodeMatchesValue(const BookmarkNode* node,
                               const base::DictionaryValue* dict) {
    base::string16 title;
    if (!dict->GetString("name", &title) || node->GetTitle() != title)
      return false;

    if (node->is_folder()) {
      const base::ListValue* children = NULL;
      if (!dict->GetList("children", &children) ||
          node->child_count() != static_cast<int>(children->GetSize())) {
        return false;
      }
      for (int i = 0; i < node->child_count(); ++i) {
        const base::DictionaryValue* child = NULL;
        if (!children->GetDictionary(i, &child) ||
            !NodeMatchesValue(node->GetChild(i), child)) {
          return false;
        }
      }
    } else if (node->is_url()) {
      std::string url;
      if (!dict->GetString("url", &url) || node->url() != url)
        return false;
    } else {
      return false;
    }
    return true;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  bookmarks::MockBookmarkModelObserver observer_;
  ManagedBookmarkService* managed_;
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarkServiceTest);
};

TEST_F(ManagedBookmarkServiceTest, EmptyManagedNode) {
  // Verifies that the managed node is empty and invisible when the policy is
  // not set.
  model_->RemoveObserver(&observer_);
  prefs_->RemoveManagedPref(bookmarks::prefs::kManagedBookmarks);
  ResetModel();

  ASSERT_TRUE(managed_->managed_node());
  EXPECT_TRUE(managed_->managed_node()->empty());
  EXPECT_FALSE(managed_->managed_node()->IsVisible());
}

TEST_F(ManagedBookmarkServiceTest, LoadInitial) {
  // Verifies that the initial load picks up the initial policy too.
  EXPECT_TRUE(model_->bookmark_bar_node()->empty());
  EXPECT_TRUE(model_->other_node()->empty());
  EXPECT_FALSE(managed_->managed_node()->empty());
  EXPECT_TRUE(managed_->managed_node()->IsVisible());

  std::unique_ptr<base::DictionaryValue> expected(CreateExpectedTree());
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected.get()));
}

TEST_F(ManagedBookmarkServiceTest, SwapNodes) {
  // Swap the Google bookmark with the Folder.
  std::unique_ptr<base::ListValue> updated(CreateTestTree());
  std::unique_ptr<base::Value> removed;
  ASSERT_TRUE(updated->Remove(0, &removed));
  updated->Append(std::move(removed));

  // These two nodes should just be swapped.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(model_, parent, 1, parent, 0));
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected.get()));
}

TEST_F(ManagedBookmarkServiceTest, RemoveNode) {
  // Remove the Folder.
  std::unique_ptr<base::ListValue> updated(CreateTestTree());
  ASSERT_TRUE(updated->Remove(1, NULL));

  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 1, _, _));
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected.get()));
}

TEST_F(ManagedBookmarkServiceTest, CreateNewNodes) {
  // Put all the nodes inside another folder.
  std::unique_ptr<base::ListValue> updated(new base::ListValue);
  updated->Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_, _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 1, _, _)).Times(2);
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->CreateDeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  std::unique_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), std::move(updated)));
  EXPECT_TRUE(NodeMatchesValue(managed_->managed_node(), expected.get()));
}

TEST_F(ManagedBookmarkServiceTest, RemoveAllUserBookmarks) {
  // Remove the policy.
  const BookmarkNode* parent = managed_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 0, _, _)).Times(2);
  prefs_->RemoveManagedPref(bookmarks::prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(managed_->managed_node()->empty());
  EXPECT_FALSE(managed_->managed_node()->IsVisible());
}

TEST_F(ManagedBookmarkServiceTest, IsDescendantOfManagedNode) {
  EXPECT_FALSE(
      bookmarks::IsDescendantOf(model_->root_node(), managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->bookmark_bar_node(),
                                         managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->other_node(),
                                         managed_->managed_node()));
  EXPECT_FALSE(bookmarks::IsDescendantOf(model_->mobile_node(),
                                         managed_->managed_node()));
  EXPECT_TRUE(bookmarks::IsDescendantOf(managed_->managed_node(),
                                        managed_->managed_node()));

  const BookmarkNode* parent = managed_->managed_node();
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(
      bookmarks::IsDescendantOf(parent->GetChild(0), managed_->managed_node()));
  EXPECT_TRUE(
      bookmarks::IsDescendantOf(parent->GetChild(1), managed_->managed_node()));

  parent = parent->GetChild(1);
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(
      bookmarks::IsDescendantOf(parent->GetChild(0), managed_->managed_node()));
  EXPECT_TRUE(
      bookmarks::IsDescendantOf(parent->GetChild(1), managed_->managed_node()));
}

TEST_F(ManagedBookmarkServiceTest, RemoveAllDoesntRemoveManaged) {
  EXPECT_EQ(2, managed_->managed_node()->child_count());

  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_, model_->bookmark_bar_node(), 0));
  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_, model_->bookmark_bar_node(), 1));
  model_->AddURL(model_->bookmark_bar_node(), 0, base::ASCIIToUTF16("Test"),
                 GURL("http://google.com/"));
  model_->AddFolder(model_->bookmark_bar_node(), 1,
                    base::ASCIIToUTF16("Test Folder"));
  EXPECT_EQ(2, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(model_, _));
  model_->RemoveAllUserBookmarks();
  EXPECT_EQ(2, managed_->managed_node()->child_count());
  EXPECT_EQ(0, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ManagedBookmarkServiceTest, HasDescendantsOfManagedNode) {
  const BookmarkNode* user_node =
      model_->AddURL(model_->other_node(), 0, base::ASCIIToUTF16("foo bar"),
                     GURL("http://www.google.com"));
  const BookmarkNode* managed_node = managed_->managed_node()->GetChild(0);
  ASSERT_TRUE(managed_node);

  std::vector<const BookmarkNode*> nodes;
  EXPECT_FALSE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
  nodes.push_back(user_node);
  EXPECT_FALSE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
  nodes.push_back(managed_node);
  EXPECT_TRUE(bookmarks::HasDescendantsOf(nodes, managed_->managed_node()));
}

TEST_F(ManagedBookmarkServiceTest, GetManagedBookmarksDomain) {
  // Not managed profile
  profile_.set_profile_name("user@google.com");
  EXPECT_TRUE(
      ManagedBookmarkServiceFactory::GetManagedBookmarksDomain(&profile_)
          .empty());

  // Managed profile
  policy::ProfilePolicyConnectorFactory::GetForBrowserContext(&profile_)
      ->OverrideIsManagedForTesting(true);
  EXPECT_EQ(
      "google.com",
      ManagedBookmarkServiceFactory::GetManagedBookmarksDomain(&profile_));
}
