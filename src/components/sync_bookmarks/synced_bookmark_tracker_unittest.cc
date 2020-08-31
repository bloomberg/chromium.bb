// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include "base/base64.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

constexpr int kNumPermanentNodes = 3;

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kMobileBookmarksId[] = "synced_bookmarks_id";
const char kOtherBookmarksId[] = "other_bookmarks_id";

// Redefinition of |enum CorruptionReason| in synced_bookmark_tracker.cc to be
// used in tests.
enum class ExpectedCorruptionReason {
  NO_CORRUPTION = 0,
  MISSING_SERVER_ID = 1,
  BOOKMARK_ID_IN_TOMBSTONE = 2,
  MISSING_BOOKMARK_ID = 3,
  DEPRECATED_COUNT_MISMATCH = 4,
  DEPRECATED_IDS_MISMATCH = 5,
  DUPLICATED_SERVER_ID = 6,
  UNKNOWN_BOOKMARK_ID = 7,
  UNTRACKED_BOOKMARK = 8,
  BOOKMARK_GUID_MISMATCH = 9,
  DUPLICATED_CLIENT_TAG_HASH = 10,
  TRACKED_MANAGED_NODE = 11,

  kMaxValue = TRACKED_MANAGED_NODE
};

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& title,
                                           const std::string& url) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_legacy_canonicalized_title(title);
  specifics.mutable_bookmark()->set_url(url);
  return specifics;
}

sync_pb::BookmarkMetadata CreateNodeMetadata(int64_t node_id,
                                             const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node_id);
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  return bookmark_metadata;
}

sync_pb::BookmarkMetadata CreateTombstoneMetadata(
    const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  bookmark_metadata.mutable_metadata()->set_is_deleted(true);
  return bookmark_metadata;
}

sync_pb::BookmarkModelMetadata CreateMetadataForPermanentNodes(
    const bookmarks::BookmarkModel* bookmark_model) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);

  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->bookmark_bar_node()->id(),
                         /*server_id=*/kBookmarkBarId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->mobile_node()->id(),
                         /*server_id=*/kMobileBookmarksId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->other_node()->id(),
                         /*server_id=*/kOtherBookmarksId);

  CHECK_EQ(kNumPermanentNodes, model_metadata.bookmarks_metadata_size());
  return model_metadata;
}

TEST(SyncedBookmarkTrackerTest, ShouldGetAssociatedNodes) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime(base::Time::Now() -
                                 base::TimeDelta::FromSeconds(1));
  const syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());

  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), kUrl);
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kCreationTime,
                   unique_position.ToProto(), specifics);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kServerVersion));
  EXPECT_THAT(entity->metadata()->creation_time(),
              Eq(syncer::TimeToProtoTime(kCreationTime)));
  EXPECT_TRUE(
      syncer::UniquePosition::FromProto(entity->metadata()->unique_position())
          .Equals(unique_position));

  syncer::EntityData data;
  *data.specifics.mutable_bookmark() = specifics.bookmark();
  data.unique_position = unique_position.ToProto();
  EXPECT_TRUE(entity->MatchesDataIgnoringParent(data));
  EXPECT_THAT(tracker->GetEntityForSyncId("unknown id"), IsNull());
}

TEST(SyncedBookmarkTrackerTest, ShouldReturnNullForDisassociatedNodes) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kModificationTime,
                   unique_position, specifics);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(tracker->GetEntityForSyncId(kSyncId), Eq(entity));
  tracker->Remove(entity);
  EXPECT_THAT(tracker->GetEntityForSyncId(kSyncId), IsNull());
}

TEST(SyncedBookmarkTrackerTest, ShouldBuildBookmarkModelMetadata) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime(base::Time::Now() -
                                 base::TimeDelta::FromSeconds(1));
  const syncer::UniquePosition unique_position =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());

  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), kUrl);
  tracker->Add(&node, kSyncId, kServerVersion, kCreationTime,
               unique_position.ToProto(), specifics);

  sync_pb::BookmarkModelMetadata bookmark_model_metadata =
      tracker->BuildBookmarkModelMetadata();

  ASSERT_THAT(bookmark_model_metadata.bookmarks_metadata().size(), Eq(1));
  EXPECT_THAT(
      bookmark_model_metadata.bookmarks_metadata(0).metadata().server_id(),
      Eq(kSyncId));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldRequireCommitRequestWhenSequenceNumberIsIncremented) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kModificationTime,
                   unique_position, specifics);

  EXPECT_THAT(tracker->HasLocalChanges(), Eq(false));
  tracker->IncrementSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(true));
  // TODO(crbug.com/516866): Test HasLocalChanges after submitting commit
  // request in a separate test probably.
}

TEST(SyncedBookmarkTrackerTest, ShouldAckSequenceNumber) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kModificationTime,
                   unique_position, specifics);

  // Test simple scenario of ack'ing an incrememented sequence number.
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(false));
  tracker->IncrementSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(true));
  tracker->AckSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(false));

  // Test ack'ing of a multiple times incremented sequence number.
  tracker->IncrementSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(true));
  tracker->IncrementSequenceNumber(entity);
  tracker->IncrementSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(true));
  tracker->AckSequenceNumber(entity);
  EXPECT_THAT(tracker->HasLocalChanges(), Eq(false));
}

TEST(SyncedBookmarkTrackerTest, ShouldUpdateUponCommitResponseWithNewId) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const std::string kNewSyncId = "NEW_SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const int64_t kNewServerVersion = 1001;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), GURL());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kModificationTime,
                   unique_position, specifics);
  ASSERT_THAT(entity, NotNull());

  // Initially only the old ID should be tracked.
  ASSERT_THAT(tracker->GetEntityForSyncId(kSyncId), Eq(entity));
  ASSERT_THAT(tracker->GetEntityForSyncId(kNewSyncId), IsNull());

  // Receive a commit response with a changed id.
  tracker->UpdateUponCommitResponse(entity, kNewSyncId, kNewServerVersion,
                                    /*acked_sequence_number=*/1);

  // Old id shouldn't be there, but the new one should.
  EXPECT_THAT(tracker->GetEntityForSyncId(kSyncId), IsNull());
  EXPECT_THAT(tracker->GetEntityForSyncId(kNewSyncId), Eq(entity));

  EXPECT_THAT(entity->metadata()->server_id(), Eq(kNewSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kNewServerVersion));
}

TEST(SyncedBookmarkTrackerTest, ShouldUpdateId) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const std::string kNewSyncId = "NEW_SYNC_ID";
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::UniquePosition unique_position;
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(/*id=*/1, base::GenerateGUID(), GURL());
  // Track a sync entity.
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kModificationTime,
                   unique_position, specifics);

  ASSERT_THAT(entity, NotNull());
  // Update the sync id.
  tracker->UpdateSyncIdForLocalCreationIfNeeded(entity, kNewSyncId);

  // Old id shouldn't be there, but the new one should.
  EXPECT_THAT(tracker->GetEntityForSyncId(kSyncId), IsNull());
  EXPECT_THAT(tracker->GetEntityForSyncId(kNewSyncId), Eq(entity));

  EXPECT_THAT(entity->metadata()->server_id(), Eq(kNewSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kServerVersion));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMaintainTombstoneOrderBetweenCtorAndBuildBookmarkModelMetadata) {
  // Feed a metadata batch of 5 entries to the constructor of the tracker.
  // First 2 are for node, and the last 4 are for tombstones.

  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";
  const std::string kId4 = "id4";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));
  const bookmarks::BookmarkNode* node1 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node1"));

  sync_pb::BookmarkModelMetadata initial_model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model.get());

  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node0->id(), /*server_id=*/kId0);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node1->id(), /*server_id=*/kId1);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/kId2);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/kId3);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/kId4);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model.get(), std::move(initial_model_metadata));
  ASSERT_THAT(tracker, NotNull());

  const sync_pb::BookmarkModelMetadata output_model_metadata =
      tracker->BuildBookmarkModelMetadata();

  // Tombstones should be the last 3 entries in the metadata and in the same
  // order as given to the constructor.
  ASSERT_THAT(output_model_metadata.bookmarks_metadata().size(),
              Eq(kNumPermanentNodes + 5));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 2)
                  .metadata()
                  .server_id(),
              Eq(kId2));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 3)
                  .metadata()
                  .server_id(),
              Eq(kId3));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 4)
                  .metadata()
                  .server_id(),
              Eq(kId4));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMaintainOrderOfMarkDeletedCallsWhenBuildBookmarkModelMetadata) {
  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";
  const std::string kId4 = "id4";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));
  const bookmarks::BookmarkNode* node1 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node1"));
  const bookmarks::BookmarkNode* node2 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node2"));
  const bookmarks::BookmarkNode* node3 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node3"));
  const bookmarks::BookmarkNode* node4 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node4"));

  sync_pb::BookmarkModelMetadata initial_model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model.get());

  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node0->id(), /*server_id=*/kId0);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node1->id(), /*server_id=*/kId1);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node2->id(), /*server_id=*/kId2);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node3->id(), /*server_id=*/kId3);
  *initial_model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node4->id(), /*server_id=*/kId4);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model.get(), std::move(initial_model_metadata));
  ASSERT_THAT(tracker, NotNull());

  // Mark entities deleted in that order kId2, kId4, kId1
  tracker->MarkDeleted(tracker->GetEntityForSyncId(kId2));
  tracker->MarkDeleted(tracker->GetEntityForSyncId(kId4));
  tracker->MarkDeleted(tracker->GetEntityForSyncId(kId1));

  const sync_pb::BookmarkModelMetadata output_model_metadata =
      tracker->BuildBookmarkModelMetadata();

  // Tombstones should be the last 3 entries in the metadata and in the same as
  // calling MarkDeleted().
  ASSERT_THAT(output_model_metadata.bookmarks_metadata().size(),
              Eq(kNumPermanentNodes + 5));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 2)
                  .metadata()
                  .server_id(),
              Eq(kId2));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 3)
                  .metadata()
                  .server_id(),
              Eq(kId4));
  EXPECT_THAT(output_model_metadata.bookmarks_metadata(kNumPermanentNodes + 4)
                  .metadata()
                  .server_id(),
              Eq(kId1));
}

TEST(SyncedBookmarkTrackerTest,
     ShouldOrderParentUpdatesBeforeChildUpdatesAndDeletionsComeLast) {
  const size_t kMaxEntries = 1000;

  // Construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));
  const bookmarks::BookmarkNode* node1 = bookmark_model->AddFolder(
      /*parent=*/node0, /*index=*/0, base::UTF8ToUTF16("node1"));
  const bookmarks::BookmarkNode* node2 = bookmark_model->AddFolder(
      /*parent=*/node1, /*index=*/0, base::UTF8ToUTF16("node2"));

  // Server ids.
  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";

  // Prepare the metadata with shuffled order.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model.get());

  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node1->id(), /*server_id=*/kId1);
  *model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/kId3);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node2->id(), /*server_id=*/kId2);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node0->id(), /*server_id=*/kId0);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model.get(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());

  // Mark the entities that they have local changes. (in shuffled order just to
  // verify the tracker doesn't simply maintain the order of updates similar to
  // with deletions).
  tracker->IncrementSequenceNumber(tracker->GetEntityForSyncId(kId3));
  tracker->IncrementSequenceNumber(tracker->GetEntityForSyncId(kId1));
  tracker->IncrementSequenceNumber(tracker->GetEntityForSyncId(kId2));
  tracker->IncrementSequenceNumber(tracker->GetEntityForSyncId(kId0));

  std::vector<const SyncedBookmarkTracker::Entity*> entities_with_local_change =
      tracker->GetEntitiesWithLocalChanges(kMaxEntries);

  ASSERT_THAT(entities_with_local_change.size(), Eq(4U));
  // Verify updates are in parent before child order node0 --> node1 --> node2.
  EXPECT_THAT(entities_with_local_change[0]->metadata()->server_id(), Eq(kId0));
  EXPECT_THAT(entities_with_local_change[1]->metadata()->server_id(), Eq(kId1));
  EXPECT_THAT(entities_with_local_change[2]->metadata()->server_id(), Eq(kId2));
  // Verify that deletion is the last entry.
  EXPECT_THAT(entities_with_local_change[3]->metadata()->server_id(), Eq(kId3));
}

TEST(SyncedBookmarkTrackerTest, ShouldMatchModelAndMetadata) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  // Add entry for the managed node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node->id(), /*server_id=*/"NodeId");

  // Add a tombstone entry.
  *model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/"tombstoneId");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              NotNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::NO_CORRUPTION, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfMissingMobileFolder) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);

  // Add entries for all the permanent nodes except for the Mobile bookmarks
  // folder.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(model->bookmark_bar_node()->id(),
                         /*server_id=*/kBookmarkBarId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(model->other_node()->id(),
                         /*server_id=*/kOtherBookmarksId);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::UNTRACKED_BOOKMARK, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfMissingServerId) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  // Remove a server ID to a permanent node.
  model_metadata.mutable_bookmarks_metadata(0)
      ->mutable_metadata()
      ->clear_server_id();

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::MISSING_SERVER_ID, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfMissingLocalBookmarkId) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  const bookmarks::BookmarkNode* node = model->AddFolder(
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16("node"));
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node->id(), /*server_id=*/"serverid");

  // Remove the local bookmark ID.
  model_metadata.mutable_bookmarks_metadata()->rbegin()->clear_id();

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::MISSING_BOOKMARK_ID, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfTombstoneHasBookmarkId) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  *model_metadata.add_bookmarks_metadata() =
      CreateTombstoneMetadata(/*server_id=*/"serverid");

  // Add a node ID to the tombstone.
  model_metadata.mutable_bookmarks_metadata()->rbegin()->set_id(1234);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::BOOKMARK_ID_IN_TOMBSTONE,
      /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfUnknownLocalBookmarkId) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  const bookmarks::BookmarkNode* node = model->AddFolder(
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      base::UTF8ToUTF16("node"));
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node->id(), /*server_id=*/"serverid");

  // Set an arbitrary local node ID that won't match anything in BookmarkModel.
  model_metadata.mutable_bookmarks_metadata()->rbegin()->set_id(123456);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::UNKNOWN_BOOKMARK_ID,
      /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest, ShouldNotMatchModelAndMetadataIfGuidMismatch) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(
      kInvalidateBookmarkSyncMetadataIfMismatchingGuid);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());
  sync_pb::BookmarkMetadata* node0_metadata =
      model_metadata.add_bookmarks_metadata();
  *node0_metadata = CreateNodeMetadata(node0->id(), /*server_id=*/"id0");

  // Set a mismatching client tag hash.
  node0_metadata->mutable_metadata()->set_client_tag_hash("corrupthash");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::BOOKMARK_GUID_MISMATCH, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfTombstoneHasDuplicatedClientTagHash) {
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());
  sync_pb::BookmarkMetadata* node0_metadata =
      model_metadata.add_bookmarks_metadata();
  *node0_metadata = CreateNodeMetadata(node0->id(), /*server_id=*/"id0");

  const syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS, node0->guid());
  node0_metadata->mutable_metadata()->set_client_tag_hash(
      client_tag_hash.value());

  // Add the duplicate tombstone with a different server id but same client tag
  // hash.
  sync_pb::BookmarkMetadata* tombstone_metadata =
      model_metadata.add_bookmarks_metadata();
  *tombstone_metadata = CreateTombstoneMetadata("id1");
  tombstone_metadata->mutable_metadata()->set_client_tag_hash(
      client_tag_hash.value());

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::DUPLICATED_CLIENT_TAG_HASH,
      /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldNotMatchModelAndMetadataIfUnsyncableNodeIsTracked) {
  // Add a managed node with an arbitrary id 100.
  const int64_t kManagedNodeId = 100;
  auto owned_managed_node = std::make_unique<bookmarks::BookmarkPermanentNode>(
      kManagedNodeId, bookmarks::BookmarkNode::FOLDER);
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

  // The model should contain the managed node now.
  ASSERT_THAT(GetBookmarkNodeByID(model.get(), kManagedNodeId),
              Eq(managed_node));

  // Add entries for all the permanent nodes. TestBookmarkClient creates all the
  // 3 permanent nodes.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  // Add unsyncable node to metadata.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(managed_node->id(),
                         /*server_id=*/"server_id");

  base::HistogramTester histogram_tester;
  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              IsNull());
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::TRACKED_MANAGED_NODE, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMatchModelAndMetadataDespiteGuidMismatch) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndDisableFeature(
      kInvalidateBookmarkSyncMetadataIfMismatchingGuid);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node0 = model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("node0"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());
  sync_pb::BookmarkMetadata* node0_metadata =
      model_metadata.add_bookmarks_metadata();
  *node0_metadata = CreateNodeMetadata(node0->id(), /*server_id=*/"id0");

  // Set a mismatching client tag hash.
  node0_metadata->mutable_metadata()->set_client_tag_hash("corrupthash");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              NotNull());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarkModelMetadataClientTagState",
      /*sample=*/0, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldMatchModelWithUnsyncableNodesAndMetadata) {
  // Add a managed node with an arbitrary id 100.
  const int64_t kManagedNodeId = 100;
  auto owned_managed_node = std::make_unique<bookmarks::BookmarkPermanentNode>(
      kManagedNodeId, bookmarks::BookmarkNode::FOLDER);
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

  // The model should contain the managed node now.
  ASSERT_THAT(GetBookmarkNodeByID(model.get(), kManagedNodeId),
              Eq(managed_node));

  // Add entries for all the permanent nodes. TestBookmarkClient creates all the
  // 3 permanent nodes.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  base::HistogramTester histogram_tester;
  EXPECT_THAT(SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
                  model.get(), std::move(model_metadata)),
              NotNull());
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksModelMetadataCorruptionReason",
      /*sample=*/ExpectedCorruptionReason::NO_CORRUPTION, /*count=*/1);
}

TEST(SyncedBookmarkTrackerTest,
     ShouldPopulateFaviconHashForNewlyAddedEntities) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());

  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime = base::Time::Now();
  const syncer::UniquePosition kUniquePosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const std::string kFaviconPngBytes = "fakefaviconbytes";

  sync_pb::EntitySpecifics specifics = GenerateSpecifics(kTitle, kUrl.spec());
  specifics.mutable_bookmark()->set_favicon(kFaviconPngBytes);

  bookmarks::BookmarkNode node(kId, base::GenerateGUID(), kUrl);
  const SyncedBookmarkTracker::Entity* entity =
      tracker->Add(&node, kSyncId, kServerVersion, kCreationTime,
                   kUniquePosition.ToProto(), specifics);

  EXPECT_TRUE(entity->metadata()->has_bookmark_favicon_hash());
  EXPECT_TRUE(entity->MatchesFaviconHash(kFaviconPngBytes));
  EXPECT_FALSE(entity->MatchesFaviconHash("otherhash"));
}

TEST(SyncedBookmarkTrackerTest, ShouldPopulateFaviconHashUponUpdate) {
  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime = base::Time::Now();
  const syncer::UniquePosition kUniquePosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());
  const std::string kFaviconPngBytes = "fakefaviconbytes";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model->AddURL(/*parent=*/bookmark_bar_node, /*index=*/0,
                    base::ASCIIToUTF16("Title"), GURL("http://www.url.com"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  // Add entry for the URL node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node->id(), kSyncId);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          model.get(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());

  const SyncedBookmarkTracker::Entity* entity =
      tracker->GetEntityForSyncId(kSyncId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->metadata()->has_bookmark_favicon_hash());
  ASSERT_FALSE(entity->MatchesFaviconHash(kFaviconPngBytes));

  sync_pb::EntitySpecifics specifics = GenerateSpecifics(kTitle, kUrl.spec());
  specifics.mutable_bookmark()->set_favicon(kFaviconPngBytes);

  tracker->Update(entity, kServerVersion, kModificationTime,
                  kUniquePosition.ToProto(), specifics);

  EXPECT_TRUE(entity->metadata()->has_bookmark_favicon_hash());
  EXPECT_TRUE(entity->MatchesFaviconHash(kFaviconPngBytes));
  EXPECT_FALSE(entity->MatchesFaviconHash("otherhash"));
}

TEST(SyncedBookmarkTrackerTest, ShouldPopulateFaviconHashExplicitly) {
  const std::string kSyncId = "SYNC_ID";
  const std::string kFaviconPngBytes = "fakefaviconbytes";

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model->AddURL(/*parent=*/bookmark_bar_node, /*index=*/0,
                    base::ASCIIToUTF16("Title"), GURL("http://www.url.com"));

  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(model.get());

  // Add entry for the URL node.
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(node->id(), kSyncId);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          model.get(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());

  const SyncedBookmarkTracker::Entity* entity =
      tracker->GetEntityForSyncId(kSyncId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->metadata()->has_bookmark_favicon_hash());
  ASSERT_FALSE(entity->MatchesFaviconHash(kFaviconPngBytes));

  tracker->PopulateFaviconHashIfUnset(entity, kFaviconPngBytes);
  EXPECT_TRUE(entity->metadata()->has_bookmark_favicon_hash());
  EXPECT_TRUE(entity->MatchesFaviconHash(kFaviconPngBytes));
  EXPECT_FALSE(entity->MatchesFaviconHash("otherhash"));

  // Further calls should be ignored.
  tracker->PopulateFaviconHashIfUnset(entity, "otherpngbytes");
  EXPECT_TRUE(entity->MatchesFaviconHash(kFaviconPngBytes));
}

}  // namespace

}  // namespace sync_bookmarks
