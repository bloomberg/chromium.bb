// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/parent_child_index.h"

#include <list>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {

namespace {

static const std::string kCacheGuid = "8HhNIHlEOCGQbIAALr9QEg==";

class ParentChildIndexTest : public testing::Test {
 public:
  virtual void TearDown() {
    // To make memory management easier, we take ownership of all EntryKernels
    // returned by our factory methods and delete them here.
    STLDeleteElements(&owned_entry_kernels_);
  }

  // Unfortunately, we can't use the regular Entry factory methods, because the
  // ParentChildIndex deals in EntryKernels.

  static syncable::Id GetBookmarkRootId() {
    return syncable::Id::CreateFromServerId("bookmark_folder");
  }

  static syncable::Id GetBookmarkId(int n) {
    return syncable::Id::CreateFromServerId("b" + base::IntToString(n));
  }

  static syncable::Id GetClientUniqueId(int n) {
    return syncable::Id::CreateFromServerId("c" + base::IntToString(n));
  }

  EntryKernel* MakeRoot() {
    // Mimics the root node.
    EntryKernel* root = new EntryKernel();
    root->put(META_HANDLE, 1);
    root->put(BASE_VERSION, -1);
    root->put(SERVER_VERSION, 0);
    root->put(IS_DIR, true);
    root->put(ID, syncable::Id());
    root->put(PARENT_ID, syncable::Id());
    root->put(SERVER_PARENT_ID, syncable::Id());

    owned_entry_kernels_.push_back(root);
    return root;
  }

  EntryKernel* MakeBookmarkRoot() {
    // Mimics a server-created bookmark folder.
    EntryKernel* folder = new EntryKernel;
    folder->put(META_HANDLE, 1);
    folder->put(BASE_VERSION, 9);
    folder->put(SERVER_VERSION, 9);
    folder->put(IS_DIR, true);
    folder->put(ID, GetBookmarkRootId());
    folder->put(SERVER_PARENT_ID, syncable::Id());
    folder->put(PARENT_ID, syncable::Id());
    folder->put(UNIQUE_SERVER_TAG, "google_chrome_bookmarks");

    owned_entry_kernels_.push_back(folder);
    return folder;
  }

  EntryKernel* MakeBookmark(int n, int pos, bool is_dir) {
    // Mimics a regular bookmark or folder.
    EntryKernel* bm = new EntryKernel();
    bm->put(META_HANDLE, n);
    bm->put(BASE_VERSION, 10);
    bm->put(SERVER_VERSION, 10);
    bm->put(IS_DIR, is_dir);
    bm->put(ID, GetBookmarkId(n));
    bm->put(PARENT_ID, GetBookmarkRootId());
    bm->put(SERVER_PARENT_ID, GetBookmarkRootId());

    bm->put(UNIQUE_BOOKMARK_TAG,
            syncable::GenerateSyncableBookmarkHash(kCacheGuid,
                                                   bm->ref(ID).GetServerId()));

    UniquePosition unique_pos =
        UniquePosition::FromInt64(pos, bm->ref(UNIQUE_BOOKMARK_TAG));
    bm->put(UNIQUE_POSITION, unique_pos);
    bm->put(SERVER_UNIQUE_POSITION, unique_pos);

    owned_entry_kernels_.push_back(bm);
    return bm;
  }

  EntryKernel* MakeUniqueClientItem(int n) {
    EntryKernel* item = new EntryKernel();
    item->put(META_HANDLE, n);
    item->put(BASE_VERSION, 10);
    item->put(SERVER_VERSION, 10);
    item->put(IS_DIR, false);
    item->put(ID, GetClientUniqueId(n));
    item->put(PARENT_ID, syncable::Id());
    item->put(SERVER_PARENT_ID, syncable::Id());
    item->put(UNIQUE_CLIENT_TAG, base::IntToString(n));

    owned_entry_kernels_.push_back(item);
    return item;
  }

  ParentChildIndex index_;

 private:
  std::list<EntryKernel*> owned_entry_kernels_;
};

TEST_F(ParentChildIndexTest, TestRootNode) {
  EntryKernel* root = MakeRoot();
  EXPECT_FALSE(ParentChildIndex::ShouldInclude(root));
}

TEST_F(ParentChildIndexTest, TestBookmarkRootFolder) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  EXPECT_TRUE(ParentChildIndex::ShouldInclude(bm_folder));
}

// Tests iteration over a set of siblings.
TEST_F(ParentChildIndexTest, ChildInsertionAndIteration) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Make some folder and non-folder entries.
  EntryKernel* b1 = MakeBookmark(1, 1, false);
  EntryKernel* b2 = MakeBookmark(2, 2, false);
  EntryKernel* b3 = MakeBookmark(3, 3, true);
  EntryKernel* b4 = MakeBookmark(4, 4, false);

  // Insert them out-of-order to test different cases.
  index_.Insert(b3); // Only child.
  index_.Insert(b4); // Right-most child.
  index_.Insert(b1); // Left-most child.
  index_.Insert(b2); // Between existing items.

  // Double-check they've been added.
  EXPECT_TRUE(index_.Contains(b1));
  EXPECT_TRUE(index_.Contains(b2));
  EXPECT_TRUE(index_.Contains(b3));
  EXPECT_TRUE(index_.Contains(b4));

  // Check the ordering.
  const OrderedChildSet* children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 4UL);
  OrderedChildSet::const_iterator it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, b3);
  it++;
  EXPECT_EQ(*it, b4);
  it++;
  EXPECT_TRUE(it == children->end());
}

// Tests iteration when hierarchy is involved.
TEST_F(ParentChildIndexTest, ChildInsertionAndIterationWithHierarchy) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeBookmark(1, 1, false);
  EntryKernel* f2 = MakeBookmark(2, 2, false);
  EntryKernel* f3 = MakeBookmark(3, 3, false);

  // Under folder f1, we have two bookmarks.
  EntryKernel* f1_b1 = MakeBookmark(101, 1, false);
  f1_b1->put(PARENT_ID, GetBookmarkId(1));
  EntryKernel* f1_b2 = MakeBookmark(102, 2, false);
  f1_b2->put(PARENT_ID, GetBookmarkId(1));

  // Under folder f2, there is one bookmark.
  EntryKernel* f2_b1 = MakeBookmark(201, 1, false);
  f2_b1->put(PARENT_ID, GetBookmarkId(2));

  // Under folder f3, there is nothing.

  // Insert in a strange order, because we can.
  index_.Insert(f1_b2);
  index_.Insert(f2);
  index_.Insert(f2_b1);
  index_.Insert(f1);
  index_.Insert(f1_b1);
  index_.Insert(f3);

  OrderedChildSet::const_iterator it;

  // Iterate over children of the bookmark root.
  const OrderedChildSet* top_children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(top_children);
  ASSERT_EQ(top_children->size(), 3UL);
  it = top_children->begin();
  EXPECT_EQ(*it, f1);
  it++;
  EXPECT_EQ(*it, f2);
  it++;
  EXPECT_EQ(*it, f3);
  it++;
  EXPECT_TRUE(it == top_children->end());

  // Iterate over children of the first folder.
  const OrderedChildSet* f1_children = index_.GetChildren(GetBookmarkId(1));
  ASSERT_TRUE(f1_children);
  ASSERT_EQ(f1_children->size(), 2UL);
  it = f1_children->begin();
  EXPECT_EQ(*it, f1_b1);
  it++;
  EXPECT_EQ(*it, f1_b2);
  it++;
  EXPECT_TRUE(it == f1_children->end());

  // Iterate over children of the second folder.
  const OrderedChildSet* f2_children = index_.GetChildren(GetBookmarkId(2));
  ASSERT_TRUE(f2_children);
  ASSERT_EQ(f2_children->size(), 1UL);
  it = f2_children->begin();
  EXPECT_EQ(*it, f2_b1);
  it++;
  EXPECT_TRUE(it == f2_children->end());

  // Check for children of the third folder.
  const OrderedChildSet* f3_children = index_.GetChildren(GetBookmarkId(3));
  EXPECT_FALSE(f3_children);
}

// Tests removing items.
TEST_F(ParentChildIndexTest, RemoveWithHierarchy) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  // Just below the root, we have folders f1 and f2.
  EntryKernel* f1 = MakeBookmark(1, 1, false);
  EntryKernel* f2 = MakeBookmark(2, 2, false);
  EntryKernel* f3 = MakeBookmark(3, 3, false);

  // Under folder f1, we have two bookmarks.
  EntryKernel* f1_b1 = MakeBookmark(101, 1, false);
  f1_b1->put(PARENT_ID, GetBookmarkId(1));
  EntryKernel* f1_b2 = MakeBookmark(102, 2, false);
  f1_b2->put(PARENT_ID, GetBookmarkId(1));

  // Under folder f2, there is one bookmark.
  EntryKernel* f2_b1 = MakeBookmark(201, 1, false);
  f2_b1->put(PARENT_ID, GetBookmarkId(2));

  // Under folder f3, there is nothing.

  // Insert in any order.
  index_.Insert(f2_b1);
  index_.Insert(f3);
  index_.Insert(f1_b2);
  index_.Insert(f1);
  index_.Insert(f2);
  index_.Insert(f1_b1);

  // Check that all are in the index.
  EXPECT_TRUE(index_.Contains(f1));
  EXPECT_TRUE(index_.Contains(f2));
  EXPECT_TRUE(index_.Contains(f3));
  EXPECT_TRUE(index_.Contains(f1_b1));
  EXPECT_TRUE(index_.Contains(f1_b2));
  EXPECT_TRUE(index_.Contains(f2_b1));

  // Remove them all in any order.
  index_.Remove(f3);
  EXPECT_FALSE(index_.Contains(f3));
  index_.Remove(f1_b2);
  EXPECT_FALSE(index_.Contains(f1_b2));
  index_.Remove(f2_b1);
  EXPECT_FALSE(index_.Contains(f2_b1));
  index_.Remove(f1);
  EXPECT_FALSE(index_.Contains(f1));
  index_.Remove(f2);
  EXPECT_FALSE(index_.Contains(f2));
  index_.Remove(f1_b1);
  EXPECT_FALSE(index_.Contains(f1_b1));
}

// Test that involves two non-ordered items.
TEST_F(ParentChildIndexTest, UnorderedChildren) {
  // Make two unique client tag items under the root node.
  EntryKernel* u1 = MakeUniqueClientItem(1);
  EntryKernel* u2 = MakeUniqueClientItem(2);

  EXPECT_FALSE(u1->ShouldMaintainPosition());
  EXPECT_FALSE(u2->ShouldMaintainPosition());

  index_.Insert(u1);
  index_.Insert(u2);

  const OrderedChildSet* children = index_.GetChildren(syncable::Id());
  EXPECT_EQ(children->count(u1), 1UL);
  EXPECT_EQ(children->count(u2), 1UL);
  EXPECT_EQ(children->size(), 2UL);
}

// Test ordered and non-ordered entries under the same parent.
// TODO(rlarocque): We should not need to support this.
TEST_F(ParentChildIndexTest, OrderedAndUnorderedChildren) {
  EntryKernel* bm_folder = MakeBookmarkRoot();
  index_.Insert(bm_folder);

  EntryKernel* b1 = MakeBookmark(1, 1, false);
  EntryKernel* b2 = MakeBookmark(2, 2, false);
  EntryKernel* u1 = MakeUniqueClientItem(1);
  u1->put(PARENT_ID, GetBookmarkRootId());

  index_.Insert(b1);
  index_.Insert(u1);
  index_.Insert(b2);

  const OrderedChildSet* children = index_.GetChildren(GetBookmarkRootId());
  ASSERT_TRUE(children);
  EXPECT_EQ(children->size(), 3UL);

  // Ensure that the non-positionable item is moved to the far right.
  OrderedChildSet::const_iterator it = children->begin();
  EXPECT_EQ(*it, b1);
  it++;
  EXPECT_EQ(*it, b2);
  it++;
  EXPECT_EQ(*it, u1);
  it++;
  EXPECT_TRUE(it == children->end());
}

}  // namespace
}  // namespace syncable
}  // namespace syncer

