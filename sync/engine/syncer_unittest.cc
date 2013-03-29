// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Syncer unit tests. Unfortunately a lot of these tests
// are outdated and need to be reworked and updated.

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "build/build_config.h"
#include "sync/engine/get_commit_ids_command.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/process_updates_command.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/engine/traffic_recorder.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/base/node_ordinal.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/syncable_delete_journal.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "sync/test/fake_encryptor.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "sync/test/fake_sync_encryption_handler.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

using std::map;
using std::multimap;
using std::set;
using std::string;

namespace syncer {

using syncable::BaseTransaction;
using syncable::Blob;
using syncable::CountEntriesWithName;
using syncable::Directory;
using syncable::Entry;
using syncable::GetFirstEntryWithName;
using syncable::GetOnlyEntryWithName;
using syncable::Id;
using syncable::kEncryptedString;
using syncable::MutableEntry;
using syncable::WriteTransaction;

using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::GET_BY_CLIENT_TAG;
using syncable::GET_BY_SERVER_TAG;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::META_HANDLE;
using syncable::MTIME;
using syncable::NON_UNIQUE_NAME;
using syncable::PARENT_ID;
using syncable::BASE_SERVER_SPECIFICS;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_ORDINAL_IN_PARENT;
using syncable::SERVER_SPECIFICS;
using syncable::SERVER_VERSION;
using syncable::UNIQUE_CLIENT_TAG;
using syncable::UNIQUE_SERVER_TAG;
using syncable::SPECIFICS;
using syncable::SYNCING;
using syncable::UNITTEST;

using sessions::StatusController;
using sessions::SyncSessionContext;
using sessions::SyncSession;

class SyncerTest : public testing::Test,
                   public SyncSession::Delegate,
                   public SyncEngineEventListener {
 protected:
  SyncerTest()
      : syncer_(NULL),
        saw_syncer_event_(false),
        traffic_recorder_(0, 0) {
}

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE {
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    last_long_poll_interval_received_ = new_interval;
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    last_short_poll_interval_received_ = new_interval;
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    last_sessions_commit_delay_seconds_ = new_delay;
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) OVERRIDE {
  }

  void GetWorkers(std::vector<ModelSafeWorker*>* out) {
    out->push_back(worker_.get());
  }

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    // We're just testing the sync engine here, so we shunt everything to
    // the SyncerThread.  Datatypes which aren't enabled aren't in the map.
    for (ModelTypeSet::Iterator it = enabled_datatypes_.First();
         it.Good(); it.Inc()) {
      (*out)[it.Get()] = GROUP_PASSIVE;
    }
  }

  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) OVERRIDE {
    DVLOG(1) << "HandleSyncEngineEvent in unittest " << event.what_happened;
    // we only test for entry-specific events, not status changed ones.
    switch (event.what_happened) {
      case SyncEngineEvent::SYNC_CYCLE_BEGIN: // Fall through.
      case SyncEngineEvent::STATUS_CHANGED:
      case SyncEngineEvent::SYNC_CYCLE_ENDED:
        return;
      default:
        CHECK(false) << "Handling unknown error type in unit tests!!";
    }
    saw_syncer_event_ = true;
  }

  SyncSession* MakeSession() {
    ModelSafeRoutingInfo info;
    GetModelSafeRoutingInfo(&info);
    ModelTypeInvalidationMap invalidation_map =
        ModelSafeRoutingInfoToInvalidationMap(info, std::string());
    sessions::SyncSourceInfo source_info(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
                                         invalidation_map);
    return new SyncSession(context_.get(), this, source_info);
  }


  void SyncShareAsDelegate(SyncSessionJob::Purpose purpose) {
    SyncerStep start;
    SyncerStep end;
    SyncSessionJob::GetSyncerStepsForPurpose(purpose, &start, &end);

    session_.reset(MakeSession());
    EXPECT_TRUE(syncer_->SyncShare(session_.get(), start, end));
  }

  void SyncShareNudge() {
    session_.reset(MakeSession());
    SyncShareAsDelegate(SyncSessionJob::NUDGE);
  }

  void SyncShareConfigure() {
    session_.reset(MakeSession());
    SyncShareAsDelegate(SyncSessionJob::CONFIGURATION);
  }

  virtual void SetUp() {
    dir_maker_.SetUp();
    mock_server_.reset(new MockConnectionManager(directory()));
    EnableDatatype(BOOKMARKS);
    EnableDatatype(NIGORI);
    EnableDatatype(PREFERENCES);
    EnableDatatype(NIGORI);
    worker_ = new FakeModelWorker(GROUP_PASSIVE);
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(this);

    ModelSafeRoutingInfo routing_info;
    std::vector<ModelSafeWorker*> workers;

    GetModelSafeRoutingInfo(&routing_info);
    GetWorkers(&workers);

    throttled_data_type_tracker_.reset(new ThrottledDataTypeTracker(NULL));

    context_.reset(
        new SyncSessionContext(
            mock_server_.get(), directory(), workers,
            &extensions_activity_monitor_, throttled_data_type_tracker_.get(),
            listeners, NULL, &traffic_recorder_,
            true,  // enable keystore encryption
            "fake_invalidator_client_id"));
    context_->set_routing_info(routing_info);
    syncer_ = new Syncer();
    session_.reset(MakeSession());

    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Directory::ChildHandles children;
    directory()->GetChildHandlesById(&trans, trans.root_id(), &children);
    ASSERT_EQ(0u, children.size());
    saw_syncer_event_ = false;
    root_id_ = TestIdFactory::root();
    parent_id_ = ids_.MakeServer("parent id");
    child_id_ = ids_.MakeServer("child id");
    directory()->set_store_birthday(mock_server_->store_birthday());
    mock_server_->SetKeystoreKey("encryption_key");
  }

  virtual void TearDown() {
    mock_server_.reset();
    delete syncer_;
    syncer_ = NULL;
    dir_maker_.TearDown();
  }
  void WriteTestDataToEntry(WriteTransaction* trans, MutableEntry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_url("http://demo/");
    specifics.mutable_bookmark()->set_favicon("PNG");
    entry->Put(syncable::SPECIFICS, specifics);
    entry->Put(syncable::IS_UNSYNCED, true);
  }
  void VerifyTestDataInEntry(BaseTransaction* trans, Entry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    VerifyTestBookmarkDataInEntry(entry);
  }
  void VerifyTestBookmarkDataInEntry(Entry* entry) {
    const sync_pb::EntitySpecifics& specifics = entry->Get(syncable::SPECIFICS);
    EXPECT_TRUE(specifics.has_bookmark());
    EXPECT_EQ("PNG", specifics.bookmark().favicon());
    EXPECT_EQ("http://demo/", specifics.bookmark().url());
  }

  void VerifyHierarchyConflictsReported(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have included a warning about hierarchy conflicts.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_TRUE(client_status.has_hierarchy_conflict_detected());
    EXPECT_TRUE(client_status.hierarchy_conflict_detected());
  }

  void VerifyNoHierarchyConflictsReported(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have reported no hierarchy conflicts detected.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_TRUE(client_status.has_hierarchy_conflict_detected());
    EXPECT_FALSE(client_status.hierarchy_conflict_detected());
  }

  void VerifyHierarchyConflictsUnspecified(
      const sync_pb::ClientToServerMessage& message) {
    // Our request should have neither confirmed nor denied hierarchy conflicts.
    const sync_pb::ClientStatus& client_status = message.client_status();
    EXPECT_FALSE(client_status.has_hierarchy_conflict_detected());
  }

  void SyncRepeatedlyToTriggerConflictResolution(SyncSession* session) {
    // We should trigger after less than 6 syncs, but extra does no harm.
    for (int i = 0 ; i < 6 ; ++i)
      syncer_->SyncShare(session, SYNCER_BEGIN, SYNCER_END);
  }
  void SyncRepeatedlyToTriggerStuckSignal(SyncSession* session) {
    // We should trigger after less than 10 syncs, but we want to avoid brittle
    // tests.
    for (int i = 0 ; i < 12 ; ++i)
      syncer_->SyncShare(session, SYNCER_BEGIN, SYNCER_END);
  }
  sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
    sync_pb::EntitySpecifics result;
    AddDefaultFieldValue(BOOKMARKS, &result);
    return result;
  }

  sync_pb::EntitySpecifics DefaultPreferencesSpecifics() {
    sync_pb::EntitySpecifics result;
    AddDefaultFieldValue(PREFERENCES, &result);
    return result;
  }
  // Enumeration of alterations to entries for commit ordering tests.
  enum EntryFeature {
    LIST_END = 0,  // Denotes the end of the list of features from below.
    SYNCED,  // Items are unsynced by default
    DELETED,
    OLD_MTIME,
    MOVED_FROM_ROOT,
  };

  struct CommitOrderingTest {
    // expected commit index.
    int commit_index;
    // Details about the item
    syncable::Id id;
    syncable::Id parent_id;
    EntryFeature features[10];

    static CommitOrderingTest MakeLastCommitItem() {
      CommitOrderingTest last_commit_item;
      last_commit_item.commit_index = -1;
      last_commit_item.id = TestIdFactory::root();
      return last_commit_item;
    }
  };

  void RunCommitOrderingTest(CommitOrderingTest* test) {
    map<int, syncable::Id> expected_positions;
    {  // Transaction scope.
      WriteTransaction trans(FROM_HERE, UNITTEST, directory());
      while (!test->id.IsRoot()) {
        if (test->commit_index >= 0) {
          map<int, syncable::Id>::value_type entry(test->commit_index,
                                                   test->id);
          bool double_position = !expected_positions.insert(entry).second;
          ASSERT_FALSE(double_position) << "Two id's expected at one position";
        }
        string utf8_name = test->id.GetServerId();
        string name(utf8_name.begin(), utf8_name.end());
        MutableEntry entry(&trans, CREATE, BOOKMARKS, test->parent_id, name);

        entry.Put(syncable::ID, test->id);
        if (test->id.ServerKnows()) {
          entry.Put(BASE_VERSION, 5);
          entry.Put(SERVER_VERSION, 5);
          entry.Put(SERVER_PARENT_ID, test->parent_id);
        }
        entry.Put(syncable::IS_DIR, true);
        entry.Put(syncable::IS_UNSYNCED, true);
        entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
        // Set the time to 30 seconds in the future to reduce the chance of
        // flaky tests.
        const base::Time& now_plus_30s =
            base::Time::Now() + base::TimeDelta::FromSeconds(30);
        const base::Time& now_minus_2h =
            base::Time::Now() - base::TimeDelta::FromHours(2);
        entry.Put(syncable::MTIME, now_plus_30s);
        for (size_t i = 0 ; i < arraysize(test->features) ; ++i) {
          switch (test->features[i]) {
            case LIST_END:
              break;
            case SYNCED:
              entry.Put(syncable::IS_UNSYNCED, false);
              break;
            case DELETED:
              entry.Put(syncable::IS_DEL, true);
              break;
            case OLD_MTIME:
              entry.Put(MTIME, now_minus_2h);
              break;
            case MOVED_FROM_ROOT:
              entry.Put(SERVER_PARENT_ID, trans.root_id());
              break;
            default:
              FAIL() << "Bad value in CommitOrderingTest list";
          }
        }
        test++;
      }
    }
    SyncShareNudge();
    ASSERT_TRUE(expected_positions.size() ==
                mock_server_->committed_ids().size());
    // If this test starts failing, be aware other sort orders could be valid.
    for (size_t i = 0; i < expected_positions.size(); ++i) {
      EXPECT_EQ(1u, expected_positions.count(i));
      EXPECT_TRUE(expected_positions[i] == mock_server_->committed_ids()[i]);
    }
  }

  void DoTruncationTest(const vector<int64>& unsynced_handle_view,
                        const vector<syncable::Id>& expected_id_order) {
    for (size_t limit = expected_id_order.size() + 2; limit > 0; --limit) {
      WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());

      ModelSafeRoutingInfo routes;
      GetModelSafeRoutingInfo(&routes);
      sessions::OrderedCommitSet output_set(routes);
      GetCommitIdsCommand command(&wtrans, limit, &output_set);
      std::set<int64> ready_unsynced_set;
      command.FilterUnreadyEntries(&wtrans, ModelTypeSet(),
                                   ModelTypeSet(), false,
                                   unsynced_handle_view, &ready_unsynced_set);
      command.BuildCommitIds(&wtrans, routes, ready_unsynced_set);
      size_t truncated_size = std::min(limit, expected_id_order.size());
      ASSERT_EQ(truncated_size, output_set.Size());
      for (size_t i = 0; i < truncated_size; ++i) {
        ASSERT_EQ(expected_id_order[i], output_set.GetCommitIdAt(i))
            << "At index " << i << " with batch size limited to " << limit;
      }
      sessions::OrderedCommitSet::Projection proj;
      proj = output_set.GetCommitIdProjection(GROUP_PASSIVE);
      ASSERT_EQ(truncated_size, proj.size());
      for (size_t i = 0; i < truncated_size; ++i) {
        SCOPED_TRACE(::testing::Message("Projection mismatch with i = ") << i);
        syncable::Id projected = output_set.GetCommitIdAt(proj[i]);
        ASSERT_EQ(expected_id_order[proj[i]], projected);
        // Since this projection is the identity, the following holds.
        ASSERT_EQ(expected_id_order[i], projected);
      }
    }
  }

  const StatusController& status() {
    return session_->status_controller();
  }

  Directory* directory() {
    return dir_maker_.directory();
  }

  const std::string local_cache_guid() {
    return directory()->cache_guid();
  }

  const std::string foreign_cache_guid() {
    return "kqyg7097kro6GSUod+GSg==";
  }

  int64 CreateUnsyncedDirectory(const string& entry_name,
      const string& idstring) {
    return CreateUnsyncedDirectory(entry_name,
        syncable::Id::CreateFromServerId(idstring));
  }

  int64 CreateUnsyncedDirectory(const string& entry_name,
      const syncable::Id& id) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(
        &wtrans, CREATE, BOOKMARKS, wtrans.root_id(), entry_name);
    EXPECT_TRUE(entry.good());
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(syncable::BASE_VERSION, id.ServerKnows() ? 1 : 0);
    entry.Put(syncable::ID, id);
    return entry.Get(META_HANDLE);
  }

  void EnableDatatype(ModelType model_type) {
    enabled_datatypes_.Put(model_type);

    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    if (context_.get()) {
      context_->set_routing_info(routing_info);
    }

    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  void DisableDatatype(ModelType model_type) {
    enabled_datatypes_.Remove(model_type);

    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    if (context_.get()) {
      context_->set_routing_info(routing_info);
    }

    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  template<typename FieldType, typename ValueType>
  ValueType GetField(int64 metahandle, FieldType field,
      ValueType default_value) {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle);
    EXPECT_TRUE(entry.good());
    if (!entry.good()) {
      return default_value;
    }
    EXPECT_EQ(metahandle, entry.Get(META_HANDLE));
    return entry.Get(field);
  }

  // Helper getters that work without a transaction, to reduce boilerplate.
  Id Get(int64 metahandle, syncable::IdField field) {
    return GetField(metahandle, field, syncable::GetNullId());
  }

  string Get(int64 metahandle, syncable::StringField field) {
    return GetField(metahandle, field, string());
  }

  int64 Get(int64 metahandle, syncable::Int64Field field) {
    return GetField(metahandle, field, syncable::kInvalidMetaHandle);
  }

  int64 Get(int64 metahandle, syncable::BaseVersion field) {
    const int64 kDefaultValue = -100;
    return GetField(metahandle, field, kDefaultValue);
  }

  bool Get(int64 metahandle, syncable::IndexedBitField field) {
    return GetField(metahandle, field, false);
  }

  bool Get(int64 metahandle, syncable::IsDelField field) {
    return GetField(metahandle, field, false);
  }

  bool Get(int64 metahandle, syncable::BitField field) {
    return GetField(metahandle, field, false);
  }

  Cryptographer* GetCryptographer(syncable::BaseTransaction* trans) {
    return directory()->GetCryptographer(trans);
  }

  MessageLoop message_loop_;

  // Some ids to aid tests. Only the root one's value is specific. The rest
  // are named for test clarity.
  // TODO(chron): Get rid of these inbuilt IDs. They only make it
  // more confusing.
  syncable::Id root_id_;
  syncable::Id parent_id_;
  syncable::Id child_id_;

  TestIdFactory ids_;

  TestDirectorySetterUpper dir_maker_;
  FakeEncryptor encryptor_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;
  scoped_ptr<ThrottledDataTypeTracker> throttled_data_type_tracker_;
  scoped_ptr<MockConnectionManager> mock_server_;

  Syncer* syncer_;

  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  bool saw_syncer_event_;
  base::TimeDelta last_short_poll_interval_received_;
  base::TimeDelta last_long_poll_interval_received_;
  base::TimeDelta last_sessions_commit_delay_seconds_;
  scoped_refptr<ModelSafeWorker> worker_;

  ModelTypeSet enabled_datatypes_;
  TrafficRecorder traffic_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SyncerTest);
};

TEST_F(SyncerTest, TestCallGatherUnsyncedEntries) {
  {
    Syncer::UnsyncedMetaHandles handles;
    {
      syncable::ReadTransaction trans(FROM_HERE, directory());
      GetUnsyncedEntries(&trans, &handles);
    }
    ASSERT_EQ(0u, handles.size());
  }
  // TODO(sync): When we can dynamically connect and disconnect the mock
  // ServerConnectionManager test disconnected GetUnsyncedEntries here. It's a
  // regression for a very old bug.
}

TEST_F(SyncerTest, GetCommitIdsCommandTruncates) {
  syncable::Id root = ids_.root();
  // Create two server entries.
  mock_server_->AddUpdateDirectory(ids_.MakeServer("x"), root, "X", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(ids_.MakeServer("w"), root, "W", 10, 10,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();

  // Create some new client entries.
  CreateUnsyncedDirectory("C", ids_.MakeLocal("c"));
  CreateUnsyncedDirectory("B", ids_.MakeLocal("b"));
  CreateUnsyncedDirectory("D", ids_.MakeLocal("d"));
  CreateUnsyncedDirectory("E", ids_.MakeLocal("e"));
  CreateUnsyncedDirectory("J", ids_.MakeLocal("j"));

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry_x(&wtrans, GET_BY_ID, ids_.MakeServer("x"));
    MutableEntry entry_b(&wtrans, GET_BY_ID, ids_.MakeLocal("b"));
    MutableEntry entry_c(&wtrans, GET_BY_ID, ids_.MakeLocal("c"));
    MutableEntry entry_d(&wtrans, GET_BY_ID, ids_.MakeLocal("d"));
    MutableEntry entry_e(&wtrans, GET_BY_ID, ids_.MakeLocal("e"));
    MutableEntry entry_w(&wtrans, GET_BY_ID, ids_.MakeServer("w"));
    MutableEntry entry_j(&wtrans, GET_BY_ID, ids_.MakeLocal("j"));
    entry_x.Put(IS_UNSYNCED, true);
    entry_b.Put(PARENT_ID, entry_x.Get(ID));
    entry_d.Put(PARENT_ID, entry_b.Get(ID));
    entry_c.Put(PARENT_ID, entry_x.Get(ID));
    entry_c.PutPredecessor(entry_b.Get(ID));
    entry_e.Put(PARENT_ID, entry_c.Get(ID));
    entry_w.PutPredecessor(entry_x.Get(ID));
    entry_w.Put(IS_UNSYNCED, true);
    entry_w.Put(SERVER_VERSION, 20);
    entry_w.Put(IS_UNAPPLIED_UPDATE, true);  // Fake a conflict.
    entry_j.PutPredecessor(entry_w.Get(ID));
  }

  // The arrangement is now: x (b (d) c (e)) w j
  // Entry "w" is in conflict, making its sucessors unready to commit.
  vector<int64> unsynced_handle_view;
  vector<syncable::Id> expected_order;
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    GetUnsyncedEntries(&rtrans, &unsynced_handle_view);
  }
  // The expected order is "x", "b", "c", "d", "e", truncated appropriately.
  expected_order.push_back(ids_.MakeServer("x"));
  expected_order.push_back(ids_.MakeLocal("b"));
  expected_order.push_back(ids_.MakeLocal("c"));
  expected_order.push_back(ids_.MakeLocal("d"));
  expected_order.push_back(ids_.MakeLocal("e"));
  DoTruncationTest(unsynced_handle_view, expected_order);
}

TEST_F(SyncerTest, GetCommitIdsFiltersThrottledEntries) {
  const ModelTypeSet throttled_types(BOOKMARKS);
  sync_pb::EntitySpecifics bookmark_data;
  AddDefaultFieldValue(BOOKMARKS, &bookmark_data);

  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(SPECIFICS, bookmark_data);
    A.Put(NON_UNIQUE_NAME, "bookmark");
  }

  // Now set the throttled types.
  context_->throttled_data_type_tracker()->SetUnthrottleTime(
      throttled_types,
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1200));
  SyncShareNudge();

  {
    // Nothing should have been committed as bookmarks is throttled.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_TRUE(entryA.Get(IS_UNSYNCED));
  }

  // Now unthrottle.
  context_->throttled_data_type_tracker()->SetUnthrottleTime(
      throttled_types,
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1200));
  SyncShareNudge();
  {
    // It should have been committed.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_FALSE(entryA.Get(IS_UNSYNCED));
  }
}

// We use a macro so we can preserve the error location.
#define VERIFY_ENTRY(id, is_unapplied, is_unsynced, prev_initialized, \
                     parent_id, version, server_version, id_fac, rtrans) \
  do { \
    Entry entryA(rtrans, syncable::GET_BY_ID, id_fac.FromNumber(id)); \
    ASSERT_TRUE(entryA.good()); \
    /* We don't use EXPECT_EQ here because when the left side param is false,
    gcc 4.6 warns about converting 'false' to pointer type for argument 1. */ \
    EXPECT_TRUE(is_unsynced == entryA.Get(IS_UNSYNCED)); \
    EXPECT_TRUE(is_unapplied == entryA.Get(IS_UNAPPLIED_UPDATE)); \
    EXPECT_TRUE(prev_initialized == \
              IsRealDataType(GetModelTypeFromSpecifics( \
                  entryA.Get(BASE_SERVER_SPECIFICS)))); \
    EXPECT_TRUE(parent_id == -1 || \
                entryA.Get(PARENT_ID) == id_fac.FromNumber(parent_id)); \
    EXPECT_EQ(version, entryA.Get(BASE_VERSION)); \
    EXPECT_EQ(server_version, entryA.Get(SERVER_VERSION)); \
  } while (0)

TEST_F(SyncerTest, GetCommitIdsFiltersUnreadyEntries) {
  KeyParams key_params = {"localhost", "dummy", "foobar"};
  KeyParams other_params = {"localhost", "dummy", "foobar2"};
  sync_pb::EntitySpecifics bookmark, encrypted_bookmark;
  bookmark.mutable_bookmark()->set_url("url");
  bookmark.mutable_bookmark()->set_title("title");
  AddDefaultFieldValue(BOOKMARKS, &encrypted_bookmark);
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateDirectory(3, 0, "C", 10, 10,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateDirectory(4, 0, "D", 10, 10,
                                   foreign_cache_guid(), "-4");
  SyncShareNudge();
  // Server side change will put A in conflict.
  mock_server_->AddUpdateDirectory(1, 0, "A", 20, 20,
                                   foreign_cache_guid(), "-1");
  {
    // Mark bookmarks as encrypted and set the cryptographer to have pending
    // keys.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    Cryptographer other_cryptographer(&encryptor_);
    other_cryptographer.AddKey(other_params);
    sync_pb::EntitySpecifics specifics;
    sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
    other_cryptographer.GetKeys(nigori->mutable_encryption_keybag());
    dir_maker_.encryption_handler()->EnableEncryptEverything();
    // Set up with an old passphrase, but have pending keys
    GetCryptographer(&wtrans)->AddKey(key_params);
    GetCryptographer(&wtrans)->Encrypt(bookmark,
                                    encrypted_bookmark.mutable_encrypted());
    GetCryptographer(&wtrans)->SetPendingKeys(nigori->encryption_keybag());

    // In conflict but properly encrypted.
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(SPECIFICS, encrypted_bookmark);
    A.Put(NON_UNIQUE_NAME, kEncryptedString);
    // Not in conflict and properly encrypted.
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNSYNCED, true);
    B.Put(SPECIFICS, encrypted_bookmark);
    B.Put(NON_UNIQUE_NAME, kEncryptedString);
    // Unencrypted specifics.
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(C.good());
    C.Put(IS_UNSYNCED, true);
    C.Put(NON_UNIQUE_NAME, kEncryptedString);
    // Unencrypted non_unique_name.
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.Put(IS_UNSYNCED, true);
    D.Put(SPECIFICS, encrypted_bookmark);
    D.Put(NON_UNIQUE_NAME, "not encrypted");
  }
  SyncShareNudge();
  {
    // Nothing should have commited due to bookmarks being encrypted and
    // the cryptographer having pending keys. A would have been resolved
    // as a simple conflict, but still be unsynced until the next sync cycle.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, true, false, 0, 20, 20, ids_, &rtrans);
    VERIFY_ENTRY(2, false, true, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, true, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, true, false, 0, 10, 10, ids_, &rtrans);

    // Resolve the pending keys.
    GetCryptographer(&rtrans)->DecryptPendingKeys(other_params);
  }
  SyncShareNudge();
  {
    // All properly encrypted and non-conflicting items should commit. "A" was
    // conflicting, but last sync cycle resolved it as simple conflict, so on
    // this sync cycle it committed succesfullly.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    // Committed successfully.
    VERIFY_ENTRY(1, false, false, false, 0, 21, 21, ids_, &rtrans);
    // Committed successfully.
    VERIFY_ENTRY(2, false, false, false, 0, 11, 11, ids_, &rtrans);
    // Was not properly encrypted.
    VERIFY_ENTRY(3, false, true, false, 0, 10, 10, ids_, &rtrans);
    // Was not properly encrypted.
    VERIFY_ENTRY(4, false, true, false, 0, 10, 10, ids_, &rtrans);
  }
  {
    // Fix the remaining items.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(C.good());
    C.Put(SPECIFICS, encrypted_bookmark);
    C.Put(NON_UNIQUE_NAME, kEncryptedString);
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.Put(SPECIFICS, encrypted_bookmark);
    D.Put(NON_UNIQUE_NAME, kEncryptedString);
  }
  SyncShareNudge();
  {
    const StatusController& status_controller = session_->status_controller();
    // Expect success.
    EXPECT_EQ(status_controller.model_neutral_state().commit_result, SYNCER_OK);
    // None should be unsynced anymore.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 21, 21, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 0, 11, 11, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 0, 11, 11, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 11, 11, ids_, &rtrans);
  }
}

TEST_F(SyncerTest, EncryptionAwareConflicts) {
  KeyParams key_params = {"localhost", "dummy", "foobar"};
  Cryptographer other_cryptographer(&encryptor_);
  other_cryptographer.AddKey(key_params);
  sync_pb::EntitySpecifics bookmark, encrypted_bookmark, modified_bookmark;
  bookmark.mutable_bookmark()->set_title("title");
  other_cryptographer.Encrypt(bookmark,
                              encrypted_bookmark.mutable_encrypted());
  AddDefaultFieldValue(BOOKMARKS, &encrypted_bookmark);
  modified_bookmark.mutable_bookmark()->set_title("title2");
  other_cryptographer.Encrypt(modified_bookmark,
                              modified_bookmark.mutable_encrypted());
  sync_pb::EntitySpecifics pref, encrypted_pref, modified_pref;
  pref.mutable_preference()->set_name("name");
  AddDefaultFieldValue(PREFERENCES, &encrypted_pref);
  other_cryptographer.Encrypt(pref,
                              encrypted_pref.mutable_encrypted());
  modified_pref.mutable_preference()->set_name("name2");
  other_cryptographer.Encrypt(modified_pref,
                              modified_pref.mutable_encrypted());
  {
    // Mark bookmarks and preferences as encrypted and set the cryptographer to
    // have pending keys.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    sync_pb::EntitySpecifics specifics;
    sync_pb::NigoriSpecifics* nigori = specifics.mutable_nigori();
    other_cryptographer.GetKeys(nigori->mutable_encryption_keybag());
    dir_maker_.encryption_handler()->EnableEncryptEverything();
    GetCryptographer(&wtrans)->SetPendingKeys(nigori->encryption_keybag());
    EXPECT_TRUE(GetCryptographer(&wtrans)->has_pending_keys());
  }

  mock_server_->AddUpdateSpecifics(1, 0, "A", 10, 10, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, "B", 10, 10, false, 2, bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, "C", 10, 10, false, 1, bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, "D", 10, 10, false, 0, pref);
  SyncShareNudge();
  {
    // Initial state. Everything is normal.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(2, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(3, false, false, false, 1, 10, 10, ids_, &rtrans);
    VERIFY_ENTRY(4, false, false, false, 0, 10, 10, ids_, &rtrans);
  }

  // Server side encryption will not be applied due to undecryptable data.
  // At this point, BASE_SERVER_SPECIFICS should be filled for all four items.
  mock_server_->AddUpdateSpecifics(1, 0, kEncryptedString, 20, 20, true, 0,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, kEncryptedString, 20, 20, false, 2,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateSpecifics(3, 1, kEncryptedString, 20, 20, false, 1,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateSpecifics(4, 0, kEncryptedString, 20, 20, false, 0,
                                   encrypted_pref,
                                   foreign_cache_guid(), "-4");
  SyncShareNudge();
  {
    // All should be unapplied due to being undecryptable and have a valid
    // BASE_SERVER_SPECIFICS.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, true, false, true, 0, 10, 20, ids_, &rtrans);
    VERIFY_ENTRY(2, true, false, true, 1, 10, 20, ids_, &rtrans);
    VERIFY_ENTRY(3, true, false, true, 1, 10, 20, ids_, &rtrans);
    VERIFY_ENTRY(4, true, false, true, 0, 10, 20, ids_, &rtrans);
  }

  // Server side change that don't modify anything should not affect
  // BASE_SERVER_SPECIFICS (such as name changes and mtime changes).
  mock_server_->AddUpdateSpecifics(1, 0, kEncryptedString, 30, 30, true, 0,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateSpecifics(2, 1, kEncryptedString, 30, 30, false, 2,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-2");
  // Item 3 doesn't change.
  mock_server_->AddUpdateSpecifics(4, 0, kEncryptedString, 30, 30, false, 0,
                                   encrypted_pref,
                                   foreign_cache_guid(), "-4");
  SyncShareNudge();
  {
    // Items 1, 2, and 4 should have newer server versions, 3 remains the same.
    // All should remain unapplied due to be undecryptable.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, true, false, true, 0, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(2, true, false, true, 1, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(3, true, false, true, 1, 10, 20, ids_, &rtrans);
    VERIFY_ENTRY(4, true, false, true, 0, 10, 30, ids_, &rtrans);
  }

  // Positional changes, parent changes, and specifics changes should reset
  // BASE_SERVER_SPECIFICS.
  // Became unencrypted.
  mock_server_->AddUpdateSpecifics(1, 0, "A", 40, 40, true, 0, bookmark,
                                   foreign_cache_guid(), "-1");
  // Reordered to after item 2.
  mock_server_->AddUpdateSpecifics(3, 1, kEncryptedString, 30, 30, false, 3,
                                   encrypted_bookmark,
                                   foreign_cache_guid(), "-3");
  SyncShareNudge();
  {
    // Items 2 and 4 should be the only ones with BASE_SERVER_SPECIFICS set.
    // Items 1 is now unencrypted, so should have applied normally.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, false, false, 0, 40, 40, ids_, &rtrans);
    VERIFY_ENTRY(2, true, false, true, 1, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(3, true, false, false, 1, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(4, true, false, true, 0, 10, 30, ids_, &rtrans);
  }

  // Make local changes, which should remain unsynced for items 2, 3, 4.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(SPECIFICS, modified_bookmark);
    A.Put(NON_UNIQUE_NAME, kEncryptedString);
    A.Put(IS_UNSYNCED, true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(SPECIFICS, modified_bookmark);
    B.Put(NON_UNIQUE_NAME, kEncryptedString);
    B.Put(IS_UNSYNCED, true);
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(C.good());
    C.Put(SPECIFICS, modified_bookmark);
    C.Put(NON_UNIQUE_NAME, kEncryptedString);
    C.Put(IS_UNSYNCED, true);
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.Put(SPECIFICS, modified_pref);
    D.Put(NON_UNIQUE_NAME, kEncryptedString);
    D.Put(IS_UNSYNCED, true);
  }
  SyncShareNudge();
  {
    // Item 1 remains unsynced due to there being pending keys.
    // Items 2, 3, 4 should remain unsynced since they were not up to date.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    VERIFY_ENTRY(1, false, true, false, 0, 40, 40, ids_, &rtrans);
    VERIFY_ENTRY(2, true, true, true, 1, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(3, true, true, false, 1, 10, 30, ids_, &rtrans);
    VERIFY_ENTRY(4, true, true, true, 0, 10, 30, ids_, &rtrans);
  }

  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    // Resolve the pending keys.
    GetCryptographer(&rtrans)->DecryptPendingKeys(key_params);
  }
  // First cycle resolves conflicts, second cycle commits changes.
  SyncShareNudge();
  EXPECT_EQ(2, status().model_neutral_state().num_server_overwrites);
  EXPECT_EQ(1, status().model_neutral_state().num_local_overwrites);
  // We successfully commited item(s).
  EXPECT_EQ(status().model_neutral_state().commit_result, SYNCER_OK);
  SyncShareNudge();

  // Everything should be resolved now. The local changes should have
  // overwritten the server changes for 2 and 4, while the server changes
  // overwrote the local for entry 3.
  EXPECT_EQ(0, status().model_neutral_state().num_server_overwrites);
  EXPECT_EQ(0, status().model_neutral_state().num_local_overwrites);
  EXPECT_EQ(status().model_neutral_state().commit_result, SYNCER_OK);
  syncable::ReadTransaction rtrans(FROM_HERE, directory());
  VERIFY_ENTRY(1, false, false, false, 0, 41, 41, ids_, &rtrans);
  VERIFY_ENTRY(2, false, false, false, 1, 31, 31, ids_, &rtrans);
  VERIFY_ENTRY(3, false, false, false, 1, 30, 30, ids_, &rtrans);
  VERIFY_ENTRY(4, false, false, false, 0, 31, 31, ids_, &rtrans);
}

#undef VERIFY_ENTRY

TEST_F(SyncerTest, TestGetUnsyncedAndSimpleCommit) {
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 1);
    WriteTestDataToEntry(&wtrans, &child);
  }

  SyncShareNudge();
  ASSERT_EQ(2u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  {
    syncable::ReadTransaction rt(FROM_HERE, directory());
    Entry entry(&rt, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(entry.good());
    VerifyTestDataInEntry(&rt, &entry);
  }
}

TEST_F(SyncerTest, TestPurgeWhileUnsynced) {
  // Similar to above, but throw a purge operation into the mix. Bug 49278.
  syncable::Id pref_node_id = TestIdFactory::MakeServer("Tim");
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 1);
    WriteTestDataToEntry(&wtrans, &child);

    MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Tim");
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_UNSYNCED, true);
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultPreferencesSpecifics());
    parent2.Put(syncable::BASE_VERSION, 1);
    parent2.Put(syncable::ID, pref_node_id);
  }

  directory()->PurgeEntriesWithTypeIn(ModelTypeSet(PREFERENCES),
                                      ModelTypeSet());

  SyncShareNudge();
  ASSERT_EQ(2U, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  {
    syncable::ReadTransaction rt(FROM_HERE, directory());
    Entry entry(&rt, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(entry.good());
    VerifyTestDataInEntry(&rt, &entry);
  }
  directory()->SaveChanges();
  {
    syncable::ReadTransaction rt(FROM_HERE, directory());
    Entry entry(&rt, syncable::GET_BY_ID, pref_node_id);
    ASSERT_FALSE(entry.good());
  }
}

TEST_F(SyncerTest, TestPurgeWhileUnapplied) {
  // Similar to above, but for unapplied items. Bug 49278.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNAPPLIED_UPDATE, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
  }

  directory()->PurgeEntriesWithTypeIn(
      ModelTypeSet(BOOKMARKS), ModelTypeSet());

  SyncShareNudge();
  directory()->SaveChanges();
  {
    syncable::ReadTransaction rt(FROM_HERE, directory());
    Entry entry(&rt, syncable::GET_BY_ID, parent_id_);
    ASSERT_FALSE(entry.good());
  }
}

TEST_F(SyncerTest, TestPurgeWithJournal) {
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, syncable::CREATE, BOOKMARKS, wtrans.root_id(),
                        "Pete");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, BOOKMARKS, parent_id_,
                       "Pete");
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 1);
    WriteTestDataToEntry(&wtrans, &child);

    MutableEntry parent2(&wtrans, syncable::CREATE, PREFERENCES,
                         wtrans.root_id(), "Tim");
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultPreferencesSpecifics());
    parent2.Put(syncable::BASE_VERSION, 1);
    parent2.Put(syncable::ID, TestIdFactory::MakeServer("Tim"));
  }

  directory()->PurgeEntriesWithTypeIn(ModelTypeSet(PREFERENCES, BOOKMARKS),
                                      ModelTypeSet(BOOKMARKS));
  {
    // Verify bookmark nodes are saved in delete journal but not preference
    // node.
    syncable::ReadTransaction rt(FROM_HERE, directory());
    syncable::DeleteJournal* delete_journal = directory()->delete_journal();
    EXPECT_EQ(2u, delete_journal->GetDeleteJournalSize(&rt));
    syncable::EntryKernelSet journal_entries;
    directory()->delete_journal()->GetDeleteJournals(&rt, BOOKMARKS,
                                                     &journal_entries);
    EXPECT_EQ(parent_id_, (*journal_entries.begin())->ref(syncable::ID));
    EXPECT_EQ(child_id_, (*journal_entries.rbegin())->ref(syncable::ID));
  }
}

TEST_F(SyncerTest, TestCommitListOrderingTwoItemsTall) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-1001), ids_.FromNumber(-1000)},
    {0, ids_.FromNumber(-1000), ids_.FromNumber(0)},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingThreeItemsTall) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-2001), ids_.FromNumber(-2000)},
    {0, ids_.FromNumber(-2000), ids_.FromNumber(0)},
    {2, ids_.FromNumber(-2002), ids_.FromNumber(-2001)},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingThreeItemsTallLimitedSize) {
  context_->set_max_commit_batch_size(2);
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-2001), ids_.FromNumber(-2000)},
    {0, ids_.FromNumber(-2000), ids_.FromNumber(0)},
    {2, ids_.FromNumber(-2002), ids_.FromNumber(-2001)},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleDeletedItem) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleUncommittedDeletedItem) {
  CommitOrderingTest items[] = {
    {-1, ids_.FromNumber(-1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest,
       TestCommitListOrderingSingleLongDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingTwoLongDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED, OLD_MTIME}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrdering3LongDeletedItemsWithSizeLimit) {
  context_->set_max_commit_batch_size(2);
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {1, ids_.FromNumber(1001), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {2, ids_.FromNumber(1002), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingTwoDeletedItemsWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingComplexDeletionScenario) {
  CommitOrderingTest items[] = {
    { 0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(0), {SYNCED}},
    {1, ids_.FromNumber(1002), ids_.FromNumber(1001), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1003), ids_.FromNumber(1001), {SYNCED}},
    {2, ids_.FromNumber(1004), ids_.FromNumber(1003), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest,
       TestCommitListOrderingComplexDeletionScenarioWith2RecentDeletes) {
  CommitOrderingTest items[] = {
    { 0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(0), {SYNCED}},
    {1, ids_.FromNumber(1002), ids_.FromNumber(1001), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1003), ids_.FromNumber(1001), {SYNCED}},
    {2, ids_.FromNumber(1004), ids_.FromNumber(1003), {DELETED}},
    {3, ids_.FromNumber(1005), ids_.FromNumber(1003), {DELETED}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingDeleteMovedItems) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {0, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED, OLD_MTIME,
                                              MOVED_FROM_ROOT}},
    CommitOrderingTest::MakeLastCommitItem(),
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingWithNesting) {
  const base::Time& now_minus_2h =
      base::Time::Now() - base::TimeDelta::FromHours(2);
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    {
      MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Bob");
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      parent.Put(syncable::ID, ids_.FromNumber(100));
      parent.Put(syncable::BASE_VERSION, 1);
      MutableEntry child(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(100), "Bob");
      ASSERT_TRUE(child.good());
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      child.Put(syncable::ID, ids_.FromNumber(101));
      child.Put(syncable::BASE_VERSION, 1);
      MutableEntry grandchild(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(101), "Bob");
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::ID, ids_.FromNumber(102));
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      grandchild.Put(syncable::BASE_VERSION, 1);
    }
    {
      // Create three deleted items which deletions we expect to be sent to the
      // server.
      MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::ID, ids_.FromNumber(103));
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      parent.Put(syncable::IS_DEL, true);
      parent.Put(syncable::BASE_VERSION, 1);
      parent.Put(syncable::MTIME, now_minus_2h);
      MutableEntry child(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(103), "Pete");
      ASSERT_TRUE(child.good());
      child.Put(syncable::ID, ids_.FromNumber(104));
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      child.Put(syncable::IS_DEL, true);
      child.Put(syncable::BASE_VERSION, 1);
      child.Put(syncable::MTIME, now_minus_2h);
      MutableEntry grandchild(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(104), "Pete");
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::ID, ids_.FromNumber(105));
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::IS_DEL, true);
      grandchild.Put(syncable::IS_DIR, false);
      grandchild.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      grandchild.Put(syncable::BASE_VERSION, 1);
      grandchild.Put(syncable::MTIME, now_minus_2h);
    }
  }

  SyncShareNudge();
  ASSERT_EQ(6u, mock_server_->committed_ids().size());
  // This test will NOT unroll deletes because SERVER_PARENT_ID is not set.
  // It will treat these like moves.
  vector<syncable::Id> commit_ids(mock_server_->committed_ids());
  EXPECT_TRUE(ids_.FromNumber(100) == commit_ids[0]);
  EXPECT_TRUE(ids_.FromNumber(101) == commit_ids[1]);
  EXPECT_TRUE(ids_.FromNumber(102) == commit_ids[2]);
  // We don't guarantee the delete orders in this test, only that they occur
  // at the end.
  std::sort(commit_ids.begin() + 3, commit_ids.end());
  EXPECT_TRUE(ids_.FromNumber(103) == commit_ids[3]);
  EXPECT_TRUE(ids_.FromNumber(104) == commit_ids[4]);
  EXPECT_TRUE(ids_.FromNumber(105) == commit_ids[5]);
}

TEST_F(SyncerTest, TestCommitListOrderingWithNewItems) {
  syncable::Id parent1_id = ids_.MakeServer("p1");
  syncable::Id parent2_id = ids_.MakeServer("p2");

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "1");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent1_id);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "2");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, parent2_id);
    parent.Put(syncable::BASE_VERSION, 1);
    child.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, parent1_id, "A");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, ids_.FromNumber(102));
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent1_id, "B");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, ids_.FromNumber(-103));
    parent.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, parent2_id, "A");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, ids_.FromNumber(-104));
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent2_id, "B");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, ids_.FromNumber(105));
    child.Put(syncable::BASE_VERSION, 1);
  }

  SyncShareNudge();
  ASSERT_EQ(6u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent1_id == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(ids_.FromNumber(102) == mock_server_->committed_ids()[2]);
  EXPECT_TRUE(ids_.FromNumber(-103) == mock_server_->committed_ids()[3]);
  EXPECT_TRUE(ids_.FromNumber(-104) == mock_server_->committed_ids()[4]);
  EXPECT_TRUE(ids_.FromNumber(105) == mock_server_->committed_ids()[5]);
}

TEST_F(SyncerTest, TestCommitListOrderingCounterexample) {
  syncable::Id child2_id = ids_.NewServerId();

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "P");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child1(&wtrans, CREATE, BOOKMARKS, parent_id_, "1");
    ASSERT_TRUE(child1.good());
    child1.Put(syncable::IS_UNSYNCED, true);
    child1.Put(syncable::ID, child_id_);
    child1.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry child2(&wtrans, CREATE, BOOKMARKS, parent_id_, "2");
    ASSERT_TRUE(child2.good());
    child2.Put(syncable::IS_UNSYNCED, true);
    child2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child2.Put(syncable::ID, child2_id);

    parent.Put(syncable::BASE_VERSION, 1);
    child1.Put(syncable::BASE_VERSION, 1);
    child2.Put(syncable::BASE_VERSION, 1);
  }

  SyncShareNudge();
  ASSERT_EQ(3u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child2_id == mock_server_->committed_ids()[2]);
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParent) {
  string parent1_name = "1";
  string parent2_name = "A";
  string child_name = "B";

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(),
                        parent1_name);
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }

  syncable::Id parent2_id = ids_.NewLocalId();
  syncable::Id child_id = ids_.NewServerId();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent2(
        &wtrans, CREATE, BOOKMARKS, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_UNSYNCED, true);
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent2.Put(syncable::ID, parent2_id);

    MutableEntry child(
        &wtrans, CREATE, BOOKMARKS, parent2_id, child_name);
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, child_id);
    child.Put(syncable::BASE_VERSION, 1);
  }

  SyncShareNudge();
  ASSERT_EQ(3u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child_id == mock_server_->committed_ids()[2]);
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    // Check that things committed correctly.
    Entry entry_1(&rtrans, syncable::GET_BY_ID, parent_id_);
    EXPECT_EQ(entry_1.Get(NON_UNIQUE_NAME), parent1_name);
    // Check that parent2 is a subfolder of parent1.
    EXPECT_EQ(1, CountEntriesWithName(&rtrans,
                                      parent_id_,
                                      parent2_name));

    // Parent2 was a local ID and thus should have changed on commit!
    Entry pre_commit_entry_parent2(&rtrans, syncable::GET_BY_ID, parent2_id);
    ASSERT_FALSE(pre_commit_entry_parent2.good());

    // Look up the new ID.
    Id parent2_committed_id =
        GetOnlyEntryWithName(&rtrans, parent_id_, parent2_name);
    EXPECT_TRUE(parent2_committed_id.ServerKnows());

    Entry child(&rtrans, syncable::GET_BY_ID, child_id);
    EXPECT_EQ(parent2_committed_id, child.Get(syncable::PARENT_ID));
  }
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParentAndChild) {
  string parent_name = "1";
  string parent2_name = "A";
  string child_name = "B";

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans,
                        CREATE, BOOKMARKS,
                        wtrans.root_id(),
                        parent_name);
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }

  int64 meta_handle_b;
  const Id parent2_local_id = ids_.NewLocalId();
  const Id child_local_id = ids_.NewLocalId();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_UNSYNCED, true);
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());

    parent2.Put(syncable::ID, parent2_local_id);
    MutableEntry child(
        &wtrans, CREATE, BOOKMARKS, parent2_local_id, child_name);
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, child_local_id);
    meta_handle_b = child.Get(syncable::META_HANDLE);
  }

  SyncShareNudge();
  ASSERT_EQ(3u, mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_local_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child_local_id == mock_server_->committed_ids()[2]);
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());

    Entry parent(&rtrans, syncable::GET_BY_ID,
                 GetOnlyEntryWithName(&rtrans, rtrans.root_id(), parent_name));
    ASSERT_TRUE(parent.good());
    EXPECT_TRUE(parent.Get(syncable::ID).ServerKnows());

    Entry parent2(&rtrans, syncable::GET_BY_ID,
                  GetOnlyEntryWithName(&rtrans, parent.Get(ID), parent2_name));
    ASSERT_TRUE(parent2.good());
    EXPECT_TRUE(parent2.Get(syncable::ID).ServerKnows());

    // Id changed on commit, so this should fail.
    Entry local_parent2_id_entry(&rtrans,
                                 syncable::GET_BY_ID,
                                 parent2_local_id);
    ASSERT_FALSE(local_parent2_id_entry.good());

    Entry entry_b(&rtrans, syncable::GET_BY_HANDLE, meta_handle_b);
    EXPECT_TRUE(entry_b.Get(syncable::ID).ServerKnows());
    EXPECT_TRUE(parent2.Get(syncable::ID) == entry_b.Get(syncable::PARENT_ID));
  }
}

TEST_F(SyncerTest, UpdateWithZeroLengthName) {
  // One illegal update
  mock_server_->AddUpdateDirectory(1, 0, "", 1, 10,
                                   foreign_cache_guid(), "-1");
  // And one legal one that we're going to delete.
  mock_server_->AddUpdateDirectory(2, 0, "FOO", 1, 10,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  // Delete the legal one. The new update has a null name.
  mock_server_->AddUpdateDirectory(2, 0, "", 2, 20,
                                   foreign_cache_guid(), "-2");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();
}

TEST_F(SyncerTest, TestBasicUpdate) {
  string id = "some_id";
  string parent_id = "0";
  string name = "in_root";
  int64 version = 10;
  int64 timestamp = 10;
  mock_server_->AddUpdateDirectory(id, parent_id, name, version, timestamp,
                                   foreign_cache_guid(), "-1");

  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    Entry entry(&trans, GET_BY_ID,
               syncable::Id::CreateFromServerId("some_id"));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DIR));
    EXPECT_TRUE(entry.Get(SERVER_VERSION) == version);
    EXPECT_TRUE(entry.Get(BASE_VERSION) == version);
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
    EXPECT_FALSE(entry.Get(IS_DEL));
  }
}

TEST_F(SyncerTest, IllegalAndLegalUpdates) {
  Id root = TestIdFactory::root();
  // Should apply just fine.
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 10, 10,
                                   foreign_cache_guid(), "-1");

  // Same name. But this SHOULD work.
  mock_server_->AddUpdateDirectory(2, 0, "in_root", 10, 10,
                                   foreign_cache_guid(), "-2");

  // Unknown parent: should never be applied. "-80" is a legal server ID,
  // because any string sent by the server is a legal server ID in the sync
  // protocol, but it's not the ID of any item known to the client.  This
  // update should succeed validation, but be stuck in the unapplied state
  // until an item with the server ID "-80" arrives.
  mock_server_->AddUpdateDirectory(3, -80, "bad_parent", 10, 10,
                                   foreign_cache_guid(), "-3");

  SyncShareNudge();

  // Id 3 should be in conflict now.
  EXPECT_EQ(1, status().TotalNumConflictingItems());
  EXPECT_EQ(1, status().num_hierarchy_conflicts());

  // The only request in that loop should have been a GetUpdate.
  // At that point, we didn't know whether or not we had conflicts.
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  VerifyHierarchyConflictsUnspecified(mock_server_->last_request());

  // These entries will be used in the second set of updates.
  mock_server_->AddUpdateDirectory(4, 0, "newer_version", 20, 10,
                                   foreign_cache_guid(), "-4");
  mock_server_->AddUpdateDirectory(5, 0, "circular1", 10, 10,
                                   foreign_cache_guid(), "-5");
  mock_server_->AddUpdateDirectory(6, 5, "circular2", 10, 10,
                                   foreign_cache_guid(), "-6");
  mock_server_->AddUpdateDirectory(9, 3, "bad_parent_child", 10, 10,
                                   foreign_cache_guid(), "-9");
  mock_server_->AddUpdateDirectory(100, 9, "bad_parent_child2", 10, 10,
                                   foreign_cache_guid(), "-100");
  mock_server_->AddUpdateDirectory(10, 0, "dir_to_bookmark", 10, 10,
                                   foreign_cache_guid(), "-10");

  SyncShareNudge();
  // The three items with an unresolved parent should be unapplied (3, 9, 100).
  // The name clash should also still be in conflict.
  EXPECT_EQ(3, status().TotalNumConflictingItems());
  EXPECT_EQ(3, status().num_hierarchy_conflicts());

  // This time around, we knew that there were conflicts.
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  VerifyHierarchyConflictsReported(mock_server_->last_request());

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    // Even though it has the same name, it should work.
    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_FALSE(name_clash.Get(IS_UNAPPLIED_UPDATE))
        << "Duplicate name SHOULD be OK.";

    Entry bad_parent(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.Get(IS_UNAPPLIED_UPDATE))
        << "child of unknown parent should be in conflict";

    Entry bad_parent_child(&trans, GET_BY_ID, ids_.FromNumber(9));
    ASSERT_TRUE(bad_parent_child.good());
    EXPECT_TRUE(bad_parent_child.Get(IS_UNAPPLIED_UPDATE))
        << "grandchild of unknown parent should be in conflict";

    Entry bad_parent_child2(&trans, GET_BY_ID, ids_.FromNumber(100));
    ASSERT_TRUE(bad_parent_child2.good());
    EXPECT_TRUE(bad_parent_child2.Get(IS_UNAPPLIED_UPDATE))
        << "great-grandchild of unknown parent should be in conflict";
  }

  // Updating 1 should not affect item 2 of the same name.
  mock_server_->AddUpdateDirectory(1, 0, "new_name", 20, 20,
                                   foreign_cache_guid(), "-1");

  // Moving 5 under 6 will create a cycle: a conflict.
  mock_server_->AddUpdateDirectory(5, 6, "circular3", 20, 20,
                                   foreign_cache_guid(), "-5");

  // Flip the is_dir bit: should fail verify & be dropped.
  mock_server_->AddUpdateBookmark(10, 0, "dir_to_bookmark", 20, 20,
                                  foreign_cache_guid(), "-10");
  SyncShareNudge();

  // Version number older than last known: should fail verify & be dropped.
  mock_server_->AddUpdateDirectory(4, 0, "old_version", 10, 10,
                                   foreign_cache_guid(), "-4");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry still_a_dir(&trans, GET_BY_ID, ids_.FromNumber(10));
    ASSERT_TRUE(still_a_dir.good());
    EXPECT_FALSE(still_a_dir.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_EQ(10u, still_a_dir.Get(BASE_VERSION));
    EXPECT_EQ(10u, still_a_dir.Get(SERVER_VERSION));
    EXPECT_TRUE(still_a_dir.Get(IS_DIR));

    Entry rename(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(rename.good());
    EXPECT_EQ(root, rename.Get(PARENT_ID));
    EXPECT_EQ("new_name", rename.Get(NON_UNIQUE_NAME));
    EXPECT_FALSE(rename.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(ids_.FromNumber(1) == rename.Get(ID));
    EXPECT_EQ(20u, rename.Get(BASE_VERSION));

    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_EQ(root, name_clash.Get(PARENT_ID));
    EXPECT_TRUE(ids_.FromNumber(2) == name_clash.Get(ID));
    EXPECT_EQ(10u, name_clash.Get(BASE_VERSION));
    EXPECT_EQ("in_root", name_clash.Get(NON_UNIQUE_NAME));

    Entry ignored_old_version(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(ignored_old_version.good());
    EXPECT_TRUE(
        ignored_old_version.Get(NON_UNIQUE_NAME) == "newer_version");
    EXPECT_FALSE(ignored_old_version.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_EQ(20u, ignored_old_version.Get(BASE_VERSION));

    Entry circular_parent_issue(&trans, GET_BY_ID, ids_.FromNumber(5));
    ASSERT_TRUE(circular_parent_issue.good());
    EXPECT_TRUE(circular_parent_issue.Get(IS_UNAPPLIED_UPDATE))
        << "circular move should be in conflict";
    EXPECT_TRUE(circular_parent_issue.Get(PARENT_ID) == root_id_);
    EXPECT_TRUE(circular_parent_issue.Get(SERVER_PARENT_ID) ==
                ids_.FromNumber(6));
    EXPECT_EQ(10u, circular_parent_issue.Get(BASE_VERSION));

    Entry circular_parent_target(&trans, GET_BY_ID, ids_.FromNumber(6));
    ASSERT_TRUE(circular_parent_target.good());
    EXPECT_FALSE(circular_parent_target.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(circular_parent_issue.Get(ID) ==
        circular_parent_target.Get(PARENT_ID));
    EXPECT_EQ(10u, circular_parent_target.Get(BASE_VERSION));
  }

  EXPECT_FALSE(saw_syncer_event_);
  EXPECT_EQ(4, status().TotalNumConflictingItems());
  EXPECT_EQ(4, status().num_hierarchy_conflicts());
}

TEST_F(SyncerTest, CommitTimeRename) {
  int64 metahandle_folder;
  int64 metahandle_new_entry;

  // Create a folder and an entry.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&trans, CREATE, BOOKMARKS, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(IS_UNSYNCED, true);
    metahandle_folder = parent.Get(META_HANDLE);

    MutableEntry entry(&trans, CREATE, BOOKMARKS, parent.Get(ID), "new_entry");
    ASSERT_TRUE(entry.good());
    metahandle_new_entry = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Mix in a directory creation too for later.
  mock_server_->AddUpdateDirectory(2, 0, "dir_in_root", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->SetCommitTimeRename("renamed_");
  SyncShareNudge();

  // Verify it was correctly renamed.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry_folder(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(entry_folder.good());
    EXPECT_EQ("renamed_Folder", entry_folder.Get(NON_UNIQUE_NAME));

    Entry entry_new(&trans, GET_BY_HANDLE, metahandle_new_entry);
    ASSERT_TRUE(entry_new.good());
    EXPECT_EQ(entry_folder.Get(ID), entry_new.Get(PARENT_ID));
    EXPECT_EQ("renamed_new_entry", entry_new.Get(NON_UNIQUE_NAME));

    // And that the unrelated directory creation worked without a rename.
    Entry new_dir(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_TRUE(new_dir.good());
    EXPECT_EQ("dir_in_root", new_dir.Get(NON_UNIQUE_NAME));
  }
}


TEST_F(SyncerTest, CommitTimeRenameI18N) {
  // This is utf-8 for the diacritized Internationalization.
  const char* i18nString = "\xc3\x8e\xc3\xb1\x74\xc3\xa9\x72\xc3\xb1"
      "\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1\xc3\xa5\x6c\xc3\xae"
      "\xc2\x9e\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1";

  int64 metahandle;
  // Create a folder, expect a commit time rename.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&trans, CREATE, BOOKMARKS, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(IS_UNSYNCED, true);
    metahandle = parent.Get(META_HANDLE);
  }

  mock_server_->SetCommitTimeRename(i18nString);
  SyncShareNudge();

  // Verify it was correctly renamed.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    string expected_folder_name(i18nString);
    expected_folder_name.append("Folder");


    Entry entry_folder(&trans, GET_BY_HANDLE, metahandle);
    ASSERT_TRUE(entry_folder.good());
    EXPECT_EQ(expected_folder_name, entry_folder.Get(NON_UNIQUE_NAME));
  }
}

// A commit with a lost response produces an update that has to be reunited with
// its parent.
TEST_F(SyncerTest, CommitReuniteUpdateAdjustsChildren) {
  // Create a folder in the root.
  int64 metahandle_folder;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "new_folder");
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_UNSYNCED, true);
    metahandle_folder = entry.Get(META_HANDLE);
  }

  // Verify it and pull the ID out of the folder.
  syncable::Id folder_id;
  int64 metahandle_entry;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(entry.good());
    folder_id = entry.Get(ID);
    ASSERT_TRUE(!folder_id.ServerKnows());
  }

  // Create an entry in the newly created folder.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, folder_id, "new_entry");
    ASSERT_TRUE(entry.good());
    metahandle_entry = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out of the entry.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(folder_id, entry.Get(PARENT_ID));
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_folder_id =
      syncable::Id::CreateFromServerId("folder_server_id");

  // The following update should cause the folder to both apply the update, as
  // well as reassociate the id.
  mock_server_->AddUpdateDirectory(new_folder_id, root_id_,
      "new_folder", new_version, timestamp,
      local_cache_guid(), folder_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Alright! Apply that update!
  SyncShareNudge();
  {
    // The folder's ID should have been updated.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry folder(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(folder.good());
    EXPECT_EQ("new_folder", folder.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(new_version == folder.Get(BASE_VERSION));
    EXPECT_TRUE(new_folder_id == folder.Get(ID));
    EXPECT_TRUE(folder.Get(ID).ServerKnows());
    EXPECT_EQ(trans.root_id(), folder.Get(PARENT_ID));

    // Since it was updated, the old folder should not exist.
    Entry old_dead_folder(&trans, GET_BY_ID, folder_id);
    EXPECT_FALSE(old_dead_folder.good());

    // The child's parent should have changed.
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(new_folder_id, entry.Get(PARENT_ID));
    EXPECT_TRUE(!entry.Get(ID).ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }
}

// A commit with a lost response produces an update that has to be reunited with
// its parent.
TEST_F(SyncerTest, CommitReuniteUpdate) {
  // Create an entry in the root.
  int64 entry_metahandle;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "new_entry");
    ASSERT_TRUE(entry.good());
    entry_metahandle = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_entry_id = syncable::Id::CreateFromServerId("server_id");

  // Generate an update from the server with a relevant ID reassignment.
  mock_server_->AddUpdateBookmark(new_entry_id, root_id_,
      "new_entry", new_version, timestamp,
      local_cache_guid(), entry_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Alright! Apply that update!
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(new_version == entry.Get(BASE_VERSION));
    EXPECT_TRUE(new_entry_id == entry.Get(ID));
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
  }
}

// A commit with a lost response must work even if the local entry was deleted
// before the update is applied. We should not duplicate the local entry in
// this case, but just create another one alongside. We may wish to examine
// this behavior in the future as it can create hanging uploads that never
// finish, that must be cleaned up on the server side after some time.
TEST_F(SyncerTest, CommitReuniteUpdateDoesNotChokeOnDeletedLocalEntry) {
  // Create a entry in the root.
  int64 entry_metahandle;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "new_entry");
    ASSERT_TRUE(entry.good());
    entry_metahandle = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }
  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_entry_id = syncable::Id::CreateFromServerId("server_id");

  // Generate an update from the server with a relevant ID reassignment.
  mock_server_->AddUpdateBookmark(new_entry_id, root_id_,
      "new_entry", new_version, timestamp,
      local_cache_guid(), entry_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Purposefully delete the entry now before the update application finishes.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    Id new_entry_id = GetOnlyEntryWithName(
        &trans, trans.root_id(), "new_entry");
    MutableEntry entry(&trans, GET_BY_ID, new_entry_id);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DEL, true);
  }

  // Just don't CHECK fail in sync, have the update split.
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Id new_entry_id = GetOnlyEntryWithName(
        &trans, trans.root_id(), "new_entry");
    Entry entry(&trans, GET_BY_ID, new_entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_DEL));

    Entry old_entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(old_entry.good());
    EXPECT_TRUE(old_entry.Get(IS_DEL));
  }
}

// TODO(chron): Add more unsanitized name tests.
TEST_F(SyncerTest, ConflictMatchingEntryHandlesUnsanitizedNames) {
  mock_server_->AddUpdateDirectory(1, 0, "A/A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, "B/B", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());

    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_VERSION, 20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_VERSION, 20);
  }
  SyncShareNudge();
  saw_syncer_event_ = false;
  mock_server_->set_conflict_all_commits(false);

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(A.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(A.Get(SERVER_VERSION) == 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(B.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(B.Get(SERVER_VERSION) == 20);
  }
}

TEST_F(SyncerTest, ConflictMatchingEntryHandlesNormalNames) {
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());

    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_VERSION, 20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_VERSION, 20);
  }
  SyncShareNudge();
  saw_syncer_event_ = false;
  mock_server_->set_conflict_all_commits(false);

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(A.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(A.Get(SERVER_VERSION) == 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(B.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(B.Get(SERVER_VERSION) == 20);
  }
}

TEST_F(SyncerTest, ReverseFolderOrderingTest) {
  mock_server_->AddUpdateDirectory(4, 3, "ggchild", 10, 10,
                                   foreign_cache_guid(), "-4");
  mock_server_->AddUpdateDirectory(3, 2, "gchild", 10, 10,
                                   foreign_cache_guid(), "-3");
  mock_server_->AddUpdateDirectory(5, 4, "gggchild", 10, 10,
                                   foreign_cache_guid(), "-5");
  mock_server_->AddUpdateDirectory(2, 1, "child", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->AddUpdateDirectory(1, 0, "parent", 10, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();
  syncable::ReadTransaction trans(FROM_HERE, directory());

  Id child_id = GetOnlyEntryWithName(
        &trans, ids_.FromNumber(4), "gggchild");
  Entry child(&trans, GET_BY_ID, child_id);
  ASSERT_TRUE(child.good());
}

class EntryCreatedInNewFolderTest : public SyncerTest {
 public:
  void CreateFolderInBob() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry bob(&trans,
                     syncable::GET_BY_ID,
                     GetOnlyEntryWithName(&trans,
                                          TestIdFactory::root(),
                                          "bob"));
    CHECK(bob.good());

    MutableEntry entry2(
        &trans, CREATE, BOOKMARKS, bob.Get(syncable::ID), "bob");
    CHECK(entry2.good());
    entry2.Put(syncable::IS_DIR, true);
    entry2.Put(syncable::IS_UNSYNCED, true);
    entry2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
  }
};

TEST_F(EntryCreatedInNewFolderTest, EntryCreatedInNewFolderMidSync) {
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
  }

  mock_server_->SetMidCommitCallback(
      base::Bind(&EntryCreatedInNewFolderTest::CreateFolderInBob,
                 base::Unretained(this)));
  syncer_->SyncShare(session_.get(), COMMIT, SYNCER_END);
  // We loop until no unsynced handles remain, so we will commit both ids.
  EXPECT_EQ(2u, mock_server_->committed_ids().size());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry parent_entry(&trans, syncable::GET_BY_ID,
        GetOnlyEntryWithName(&trans, TestIdFactory::root(), "bob"));
    ASSERT_TRUE(parent_entry.good());

    Id child_id =
        GetOnlyEntryWithName(&trans, parent_entry.Get(ID), "bob");
    Entry child(&trans, syncable::GET_BY_ID, child_id);
    ASSERT_TRUE(child.good());
    EXPECT_EQ(parent_entry.Get(ID), child.Get(PARENT_ID));
  }
}

TEST_F(SyncerTest, NegativeIDInUpdate) {
  mock_server_->AddUpdateBookmark(-10, 0, "bad", 40, 40,
                                  foreign_cache_guid(), "-100");
  SyncShareNudge();
  // The negative id would make us CHECK!
}

TEST_F(SyncerTest, UnappliedUpdateOnCreatedItemItemDoesNotCrash) {
  int64 metahandle_fred;
  syncable::Id orig_id;
  {
    // Create an item.
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry fred_match(&trans, CREATE, BOOKMARKS, trans.root_id(),
                            "fred_match");
    ASSERT_TRUE(fred_match.good());
    metahandle_fred = fred_match.Get(META_HANDLE);
    orig_id = fred_match.Get(ID);
    WriteTestDataToEntry(&trans, &fred_match);
  }
  // Commit it.
  SyncShareNudge();
  EXPECT_EQ(1u, mock_server_->committed_ids().size());
  mock_server_->set_conflict_all_commits(true);
  syncable::Id fred_match_id;
  {
    // Now receive a change from outside.
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry fred_match(&trans, GET_BY_HANDLE, metahandle_fred);
    ASSERT_TRUE(fred_match.good());
    EXPECT_TRUE(fred_match.Get(ID).ServerKnows());
    fred_match_id = fred_match.Get(ID);
    mock_server_->AddUpdateBookmark(fred_match_id, trans.root_id(),
        "fred_match", 40, 40, local_cache_guid(), orig_id.GetServerId());
  }
  // Run the syncer.
  for (int i = 0 ; i < 30 ; ++i) {
    SyncShareNudge();
  }
}

/**
 * In the event that we have a double changed entry, that is changed on both
 * the client and the server, the conflict resolver should just drop one of
 * them and accept the other.
 */

TEST_F(SyncerTest, DoublyChangedWithResolver) {
  syncable::Id local_id;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 5);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete.htm");
    ASSERT_TRUE(child.good());
    local_id = child.Get(syncable::ID);
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 10);
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "Pete2.htm", 11, 10,
                                  local_cache_guid(), local_id.GetServerId());
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  syncable::Directory::ChildHandles children;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    directory()->GetChildHandlesById(&trans, parent_id_, &children);
    // We expect the conflict resolver to preserve the local entry.
    Entry child(&trans, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(child.good());
    EXPECT_TRUE(child.Get(syncable::IS_UNSYNCED));
    EXPECT_FALSE(child.Get(syncable::IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(child.Get(SPECIFICS).has_bookmark());
    EXPECT_EQ("Pete.htm", child.Get(NON_UNIQUE_NAME));
    VerifyTestBookmarkDataInEntry(&child);
  }

  // Only one entry, since we just overwrite one.
  EXPECT_EQ(1u, children.size());
  saw_syncer_event_ = false;
}

// We got this repro case when someone was editing bookmarks while sync was
// occuring. The entry had changed out underneath the user.
TEST_F(SyncerTest, CommitsUpdateDoesntAlterEntry) {
  const base::Time& test_time = ProtoTimeToTime(123456);
  syncable::Id local_id;
  int64 entry_metahandle;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, CREATE, BOOKMARKS, root_id_, "Pete");
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(ID).ServerKnows());
    local_id = entry.Get(ID);
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::MTIME, test_time);
    entry_metahandle = entry.Get(META_HANDLE);
  }
  SyncShareNudge();
  syncable::Id id;
  int64 version;
  NodeOrdinal server_ordinal_in_parent;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    id = entry.Get(ID);
    EXPECT_TRUE(id.ServerKnows());
    version = entry.Get(BASE_VERSION);
    server_ordinal_in_parent = entry.Get(SERVER_ORDINAL_IN_PARENT);
  }
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id.GetServerId());
  EXPECT_EQ("Pete", update->name());
  EXPECT_EQ(id.GetServerId(), update->id_string());
  EXPECT_EQ(root_id_.GetServerId(), update->parent_id_string());
  EXPECT_EQ(version, update->version());
  EXPECT_EQ(
      NodeOrdinalToInt64(server_ordinal_in_parent),
      update->position_in_parent());
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(MTIME) == test_time);
  }
}

TEST_F(SyncerTest, ParentAndChildBothMatch) {
  const FullModelTypeSet all_types = FullModelTypeSet::All();
  syncable::Id parent_id = ids_.NewServerId();
  syncable::Id child_id = ids_.NewServerId();
  syncable::Id parent_local_id;
  syncable::Id child_local_id;


  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent_local_id = parent.Get(ID);
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    parent.Put(ID, parent_id);
    parent.Put(BASE_VERSION, 1);
    parent.Put(SPECIFICS, DefaultBookmarkSpecifics());

    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.Get(ID), "test.htm");
    ASSERT_TRUE(child.good());
    child_local_id = child.Get(ID);
    child.Put(ID, child_id);
    child.Put(BASE_VERSION, 1);
    child.Put(SPECIFICS, DefaultBookmarkSpecifics());
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateDirectory(parent_id, root_id_, "Folder", 10, 10,
                                   local_cache_guid(),
                                   parent_local_id.GetServerId());
  mock_server_->AddUpdateBookmark(child_id, parent_id, "test.htm", 10, 10,
                                  local_cache_guid(),
                                  child_local_id.GetServerId());
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  SyncShareNudge();
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Directory::ChildHandles children;
    directory()->GetChildHandlesById(&trans, root_id_, &children);
    EXPECT_EQ(1u, children.size());
    directory()->GetChildHandlesById(&trans, parent_id, &children);
    EXPECT_EQ(1u, children.size());
    std::vector<int64> unapplied;
    directory()->GetUnappliedUpdateMetaHandles(&trans, all_types, &unapplied);
    EXPECT_EQ(0u, unapplied.size());
    syncable::Directory::UnsyncedMetaHandles unsynced;
    directory()->GetUnsyncedMetaHandles(&trans, &unsynced);
    EXPECT_EQ(0u, unsynced.size());
    saw_syncer_event_ = false;
  }
}

TEST_F(SyncerTest, CommittingNewDeleted) {
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_DEL, true);
  }
  SyncShareNudge();
  EXPECT_EQ(0u, mock_server_->committed_ids().size());
}

// Original problem synopsis:
// Check failed: entry->Get(BASE_VERSION) <= entry->Get(SERVER_VERSION)
// Client creates entry, client finishes committing entry. Between
// commit and getting update back, we delete the entry.
// We get the update for the entry, but the local one was modified
// so we store the entry but don't apply it. IS_UNAPPLIED_UPDATE is set.
// We commit deletion and get a new version number.
// We apply unapplied updates again before we get the update about the deletion.
// This means we have an unapplied update where server_version < base_version.
TEST_F(SyncerTest, UnappliedUpdateDuringCommit) {
  // This test is a little fake.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    entry.Put(ID, ids_.FromNumber(20));
    entry.Put(BASE_VERSION, 1);
    entry.Put(SERVER_VERSION, 1);
    entry.Put(SERVER_PARENT_ID, ids_.FromNumber(9999));  // Bad parent.
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_UNAPPLIED_UPDATE, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(SERVER_SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_DEL, false);
  }
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);
  EXPECT_EQ(1, session_->status_controller().TotalNumConflictingItems());
  saw_syncer_event_ = false;
}

// Original problem synopsis:
//   Illegal parent
// Unexpected error during sync if we:
//   make a new folder bob
//   wait for sync
//   make a new folder fred
//   move bob into fred
//   remove bob
//   remove fred
// if no syncing occured midway, bob will have an illegal parent
TEST_F(SyncerTest, DeletingEntryInFolder) {
  // This test is a little fake.
  int64 existing_metahandle;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "existing");
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_UNSYNCED, true);
    existing_metahandle = entry.Get(META_HANDLE);
  }
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry newfolder(&trans, CREATE, BOOKMARKS, trans.root_id(), "new");
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_DIR, true);
    newfolder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    newfolder.Put(IS_UNSYNCED, true);

    MutableEntry existing(&trans, GET_BY_HANDLE, existing_metahandle);
    ASSERT_TRUE(existing.good());
    existing.Put(PARENT_ID, newfolder.Get(ID));
    existing.Put(IS_UNSYNCED, true);
    EXPECT_TRUE(existing.Get(ID).ServerKnows());

    newfolder.Put(IS_DEL, true);
    existing.Put(IS_DEL, true);
  }
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);
  EXPECT_EQ(0, status().num_server_conflicts());
}

TEST_F(SyncerTest, DeletingEntryWithLocalEdits) {
  int64 newfolder_metahandle;

  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry newfolder(
        &trans, CREATE, BOOKMARKS, ids_.FromNumber(1), "local");
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_UNSYNCED, true);
    newfolder.Put(IS_DIR, true);
    newfolder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    newfolder_metahandle = newfolder.Get(META_HANDLE);
  }
  mock_server_->AddUpdateDirectory(1, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, APPLY_UPDATES);
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_HANDLE, newfolder_metahandle);
    ASSERT_TRUE(entry.good());
  }
}

TEST_F(SyncerTest, FolderSwapUpdate) {
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-7801");
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10,
                                   foreign_cache_guid(), "-1024");
  SyncShareNudge();
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-1024");
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20,
                                   foreign_cache_guid(), "-7801");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("fred" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
  }
  saw_syncer_event_ = false;
}

TEST_F(SyncerTest, NameCollidingFolderSwapWorksFine) {
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-7801");
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10,
                                   foreign_cache_guid(), "-1024");
  mock_server_->AddUpdateDirectory(4096, 0, "alice", 1, 10,
                                   foreign_cache_guid(), "-4096");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("bob" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("fred" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("alice" == id3.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-1024");
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20,
                                   foreign_cache_guid(), "-7801");
  mock_server_->AddUpdateDirectory(4096, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-4096");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("fred" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("bob" == id3.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  saw_syncer_event_ = false;
}

// Committing more than kDefaultMaxCommitBatchSize items requires that
// we post more than one commit command to the server.  This test makes
// sure that scenario works as expected.
TEST_F(SyncerTest, CommitManyItemsInOneGo_Success) {
  uint32 num_batches = 3;
  uint32 items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32 i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::StringPrintf("%d", i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
      e.Put(SPECIFICS, DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  SyncShareNudge();
  EXPECT_EQ(num_batches, mock_server_->commit_messages().size());
  EXPECT_EQ(0, directory()->unsynced_entity_count());
}

// Test that a single failure to contact the server will cause us to exit the
// commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_PostBufferFail) {
  uint32 num_batches = 3;
  uint32 items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32 i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::StringPrintf("%d", i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
      e.Put(SPECIFICS, DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  // The second commit should fail.  It will be preceded by one successful
  // GetUpdate and one succesful commit.
  mock_server_->FailNthPostBufferToPathCall(3);
  SyncShareNudge();

  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  EXPECT_EQ(SYNC_SERVER_ERROR,
            session_->status_controller().model_neutral_state().commit_result);
  EXPECT_EQ(items_to_commit - kDefaultMaxCommitBatchSize,
            directory()->unsynced_entity_count());
}

// Test that a single conflict response from the server will cause us to exit
// the commit loop immediately.
TEST_F(SyncerTest, CommitManyItemsInOneGo_CommitConflict) {
  uint32 num_batches = 2;
  uint32 items_to_commit = kDefaultMaxCommitBatchSize * num_batches;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (uint32 i = 0; i < items_to_commit; i++) {
      string nameutf8 = base::StringPrintf("%d", i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, BOOKMARKS, trans.root_id(), name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
      e.Put(SPECIFICS, DefaultBookmarkSpecifics());
    }
  }
  ASSERT_EQ(items_to_commit, directory()->unsynced_entity_count());

  // Return a CONFLICT response for the first item.
  mock_server_->set_conflict_n_commits(1);
  SyncShareNudge();

  // We should stop looping at the first sign of trouble.
  EXPECT_EQ(1U, mock_server_->commit_messages().size());
  EXPECT_EQ(items_to_commit - (kDefaultMaxCommitBatchSize - 1),
            directory()->unsynced_entity_count());
}

TEST_F(SyncerTest, HugeConflict) {
  int item_count = 300;  // We should be able to do 300 or 3000 w/o issue.

  syncable::Id parent_id = ids_.NewServerId();
  syncable::Id last_id = parent_id;
  vector<syncable::Id> tree_ids;

  // Create a lot of updates for which the parent does not exist yet.
  // Generate a huge deep tree which should all fail to apply at first.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    for (int i = 0; i < item_count ; i++) {
      syncable::Id next_id = ids_.NewServerId();
      syncable::Id local_id = ids_.NewLocalId();
      tree_ids.push_back(next_id);
      mock_server_->AddUpdateDirectory(next_id, last_id, "BOB", 2, 20,
                                       foreign_cache_guid(),
                                       local_id.GetServerId());
      last_id = next_id;
    }
  }
  SyncShareNudge();

  // Check they're in the expected conflict state.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      // They should all exist but none should be applied.
      ASSERT_TRUE(e.good());
      EXPECT_TRUE(e.Get(IS_DEL));
      EXPECT_TRUE(e.Get(IS_UNAPPLIED_UPDATE));
    }
  }

  // Add the missing parent directory.
  mock_server_->AddUpdateDirectory(parent_id, TestIdFactory::root(),
      "BOB", 2, 20, foreign_cache_guid(), "-3500");
  SyncShareNudge();

  // Now they should all be OK.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      ASSERT_TRUE(e.good());
      EXPECT_FALSE(e.Get(IS_DEL));
      EXPECT_FALSE(e.Get(IS_UNAPPLIED_UPDATE));
    }
  }
}

TEST_F(SyncerTest, DontCrashOnCaseChange) {
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry e(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(e.good());
    e.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "BOB", 2, 20,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();  // USED TO CAUSE AN ASSERT
  saw_syncer_event_ = false;
}

TEST_F(SyncerTest, UnsyncedItemAndUpdate) {
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(2, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();  // USED TO CAUSE AN ASSERT
  saw_syncer_event_ = false;
}

TEST_F(SyncerTest, NewEntryAndAlteredServerEntrySharePath) {
  mock_server_->AddUpdateBookmark(1, 0, "Foo.htm", 10, 10,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  int64 local_folder_handle;
  syncable::Id local_folder_id;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry new_entry(
        &wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Bar.htm");
    ASSERT_TRUE(new_entry.good());
    local_folder_id = new_entry.Get(ID);
    local_folder_handle = new_entry.Get(META_HANDLE);
    new_entry.Put(IS_UNSYNCED, true);
    new_entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry old(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(old.good());
    WriteTestDataToEntry(&wtrans, &old);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 20, 20,
                                  foreign_cache_guid(), "-1");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  saw_syncer_event_ = false;
  {
    // Update #20 should have been dropped in favor of the local version.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(server.Get(IS_UNSYNCED));
    EXPECT_TRUE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Foo.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
  }
  // Allow local changes to commit.
  mock_server_->set_conflict_all_commits(false);
  SyncShareNudge();
  saw_syncer_event_ = false;

  // Now add a server change to make the two names equal.  There should
  // be no conflict with that, since names are not unique.
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 30, 30,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  saw_syncer_event_ = false;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(server.Get(IS_UNSYNCED));
    EXPECT_FALSE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Bar.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.Get(SPECIFICS).bookmark().url());
  }
}

// Same as NewEntryAnddServerEntrySharePath, but using the old-style protocol.
TEST_F(SyncerTest, NewEntryAndAlteredServerEntrySharePath_OldBookmarksProto) {
  mock_server_->set_use_legacy_bookmarks_protocol(true);
  mock_server_->AddUpdateBookmark(1, 0, "Foo.htm", 10, 10,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  int64 local_folder_handle;
  syncable::Id local_folder_id;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry new_entry(
        &wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Bar.htm");
    ASSERT_TRUE(new_entry.good());
    local_folder_id = new_entry.Get(ID);
    local_folder_handle = new_entry.Get(META_HANDLE);
    new_entry.Put(IS_UNSYNCED, true);
    new_entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry old(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(old.good());
    WriteTestDataToEntry(&wtrans, &old);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 20, 20,
                                  foreign_cache_guid(), "-1");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  saw_syncer_event_ = false;
  {
    // Update #20 should have been dropped in favor of the local version.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(server.Get(IS_UNSYNCED));
    EXPECT_TRUE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Foo.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
  }
  // Allow local changes to commit.
  mock_server_->set_conflict_all_commits(false);
  SyncShareNudge();
  saw_syncer_event_ = false;

  // Now add a server change to make the two names equal.  There should
  // be no conflict with that, since names are not unique.
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 30, 30,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  saw_syncer_event_ = false;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(server.Get(IS_UNSYNCED));
    EXPECT_FALSE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Bar.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.Get(SPECIFICS).bookmark().url());
  }
}

// Circular links should be resolved by the server.
TEST_F(SyncerTest, SiblingDirectoriesBecomeCircular) {
  // we don't currently resolve this. This test ensures we don't.
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(PARENT_ID, ids_.FromNumber(2)));
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "B"));
  }
  mock_server_->AddUpdateDirectory(2, 1, "A", 20, 20,
                                   foreign_cache_guid(), "-2");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  saw_syncer_event_ = false;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(A.Get(NON_UNIQUE_NAME) == "B");
    EXPECT_TRUE(B.Get(NON_UNIQUE_NAME) == "B");
  }
}

TEST_F(SyncerTest, SwapEntryNames) {
  // Simple transaction test.
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "C"));
    ASSERT_TRUE(B.Put(NON_UNIQUE_NAME, "A"));
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "B"));
  }
  SyncShareNudge();
  saw_syncer_event_ = false;
}

TEST_F(SyncerTest, DualDeletionWithNewItemNameClash) {
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateBookmark(2, 0, "B", 10, 10,
                                  foreign_cache_guid(), "-2");
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    WriteTestDataToEntry(&trans, &B);
    B.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateBookmark(2, 0, "A", 11, 11,
                                  foreign_cache_guid(), "-2");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_FALSE(B.Get(IS_UNSYNCED));
    EXPECT_FALSE(B.Get(IS_UNAPPLIED_UPDATE));
  }
  saw_syncer_event_ = false;
}

// When we undelete an entity as a result of conflict resolution, we reuse the
// existing server id and preserve the old version, simply updating the server
// version with the new non-deleted entity.
TEST_F(SyncerTest, ResolveWeWroteTheyDeleted) {
  int64 bob_metahandle;

  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob_metahandle = bob.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &bob);
  }
  mock_server_->AddUpdateBookmark(1, 0, "bob", 2, 10,
                                  foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry bob(&trans, GET_BY_HANDLE, bob_metahandle);
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(ID).ServerKnows());
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_DEL));
    EXPECT_EQ(2, bob.Get(SERVER_VERSION));
    EXPECT_EQ(2, bob.Get(BASE_VERSION));
  }
  saw_syncer_event_ = false;
}

// This test is to reproduce a check failure. Sometimes we would get a bad ID
// back when creating an entry.
TEST_F(SyncerTest, DuplicateIDReturn) {
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry folder(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    ASSERT_TRUE(folder.good());
    folder.Put(IS_UNSYNCED, true);
    folder.Put(IS_DIR, true);
    folder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry folder2(&trans, CREATE, BOOKMARKS, trans.root_id(), "fred");
    ASSERT_TRUE(folder2.good());
    folder2.Put(IS_UNSYNCED, false);
    folder2.Put(IS_DIR, true);
    folder2.Put(SPECIFICS, DefaultBookmarkSpecifics());
    folder2.Put(BASE_VERSION, 3);
    folder2.Put(ID, syncable::Id::CreateFromServerId("mock_server:10000"));
  }
  mock_server_->set_next_new_id(10000);
  EXPECT_EQ(1u, directory()->unsynced_entity_count());
  // we get back a bad id in here (should never happen).
  SyncShareNudge();
  EXPECT_EQ(1u, directory()->unsynced_entity_count());
  SyncShareNudge();  // another bad id in here.
  EXPECT_EQ(0u, directory()->unsynced_entity_count());
  saw_syncer_event_ = false;
}

TEST_F(SyncerTest, DeletedEntryWithBadParentInLoopCalculation) {
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10,
                                   foreign_cache_guid(), "-1");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    // This is valid, because the parent could have gone away a long time ago.
    bob.Put(PARENT_ID, ids_.FromNumber(54));
    bob.Put(IS_DEL, true);
    bob.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 1, 10,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  SyncShareNudge();
}

TEST_F(SyncerTest, ConflictResolverMergesLocalDeleteAndServerUpdate) {
  syncable::Id local_id;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());

    MutableEntry local_deleted(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "name");
    local_id = local_deleted.Get(ID);
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_DIR, false);
    local_deleted.Put(IS_UNSYNCED, true);
    local_deleted.Put(SPECIFICS, DefaultBookmarkSpecifics());
  }

  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10,
                                  local_cache_guid(),
                                  local_id.GetServerId());

  // We don't care about actually committing, just the resolution.
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry local_deleted(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_TRUE(local_deleted.Get(BASE_VERSION) == 10);
    EXPECT_TRUE(local_deleted.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(local_deleted.Get(IS_UNSYNCED) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DEL) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DIR) == false);
  }
}

// See what happens if the IS_DIR bit gets flipped.  This can cause us
// all kinds of disasters.
TEST_F(SyncerTest, UpdateFlipsTheFolderBit) {
  // Local object: a deleted directory (container), revision 1, unsynced.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());

    MutableEntry local_deleted(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "name");
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_DIR, true);
    local_deleted.Put(IS_UNSYNCED, true);
    local_deleted.Put(SPECIFICS, DefaultBookmarkSpecifics());
  }

  // Server update: entry-type object (not a container), revision 10.
  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10,
                                  local_cache_guid(),
                                  ids_.FromNumber(1).GetServerId());

  // Don't attempt to commit.
  mock_server_->set_conflict_all_commits(true);

  // The syncer should not attempt to apply the invalid update.
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry local_deleted(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_TRUE(local_deleted.Get(BASE_VERSION) == 1);
    EXPECT_TRUE(local_deleted.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(local_deleted.Get(IS_UNSYNCED) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DEL) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DIR) == true);
  }
}

// Bug Synopsis:
// Merge conflict resolution will merge a new local entry with another entry
// that needs updates, resulting in CHECK.
TEST_F(SyncerTest, MergingExistingItems) {
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10,
                                  local_cache_guid(), "-1");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "Copy of base");
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50,
                                  local_cache_guid(), "-1");
  SyncRepeatedlyToTriggerConflictResolution(session_.get());
}

// In this test a long changelog contains a child at the start of the changelog
// and a parent at the end. While these updates are in progress the client would
// appear stuck.
TEST_F(SyncerTest, LongChangelistWithApplicationConflict) {
  const int depth = 400;
  syncable::Id folder_id = ids_.FromNumber(1);

  // First we an item in a folder in the root. However the folder won't come
  // till much later.
  syncable::Id stuck_entry_id = TestIdFactory::FromNumber(99999);
  mock_server_->AddUpdateDirectory(stuck_entry_id,
      folder_id, "stuck", 1, 1,
      foreign_cache_guid(), "-99999");
  mock_server_->SetChangesRemaining(depth - 1);
  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);

  // Buffer up a very long series of downloads.
  // We should never be stuck (conflict resolution shouldn't
  // kick in so long as we're making forward progress).
  for (int i = 0; i < depth; i++) {
    mock_server_->NextUpdateBatch();
    mock_server_->SetNewTimestamp(i + 1);
    mock_server_->SetChangesRemaining(depth - i);
  }

  syncer_->SyncShare(session_.get(), SYNCER_BEGIN, SYNCER_END);

  // Ensure our folder hasn't somehow applied.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry child(&trans, GET_BY_ID, stuck_entry_id);
    EXPECT_TRUE(child.good());
    EXPECT_TRUE(child.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(child.Get(IS_DEL));
    EXPECT_FALSE(child.Get(IS_UNSYNCED));
  }

  // And finally the folder.
  mock_server_->AddUpdateDirectory(folder_id,
      TestIdFactory::root(), "folder", 1, 1,
      foreign_cache_guid(), "-1");
  mock_server_->SetChangesRemaining(0);
  SyncShareNudge();
  SyncShareNudge();
  // Check that everything is as expected after the commit.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, folder_id);
    ASSERT_TRUE(entry.good());
    Entry child(&trans, GET_BY_ID, stuck_entry_id);
    EXPECT_EQ(entry.Get(ID), child.Get(PARENT_ID));
    EXPECT_EQ("stuck", child.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(child.good());
  }
}

TEST_F(SyncerTest, DontMergeTwoExistingItems) {
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10,
                                  foreign_cache_guid(), "-1");
  mock_server_->AddUpdateBookmark(2, 0, "base2", 10, 10,
                                  foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(NON_UNIQUE_NAME, "Copy of base"));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50,
                                  foreign_cache_guid(), "-1");
  SyncRepeatedlyToTriggerConflictResolution(session_.get());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry1(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_FALSE(entry1.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry1.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry1.Get(IS_DEL));
    Entry entry2(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_FALSE(entry2.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(entry2.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry2.Get(IS_DEL));
    EXPECT_EQ(entry1.Get(NON_UNIQUE_NAME), entry2.Get(NON_UNIQUE_NAME));
  }
}

TEST_F(SyncerTest, TestUndeleteUpdate) {
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 1, "bar", 1, 2,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  mock_server_->AddUpdateDirectory(2, 1, "bar", 2, 3,
                                   foreign_cache_guid(), "-2");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();

  int64 metahandle;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DEL));
    metahandle = entry.Get(META_HANDLE);
  }
  mock_server_->AddUpdateDirectory(1, 0, "foo", 2, 4,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();
  // This used to be rejected as it's an undeletion. Now, it results in moving
  // the delete path aside.
  mock_server_->AddUpdateDirectory(2, 1, "bar", 3, 5,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
    EXPECT_TRUE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_NE(entry.Get(META_HANDLE), metahandle);
  }
}

TEST_F(SyncerTest, TestMoveSanitizedNamedFolder) {
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 0, ":::", 1, 2,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(PARENT_ID, ids_.FromNumber(1)));
    EXPECT_TRUE(entry.Put(IS_UNSYNCED, true));
  }
  SyncShareNudge();
  // We use the same sync ts as before so our times match up.
  mock_server_->AddUpdateDirectory(2, 1, ":::", 2, 2,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
}

// Don't crash when this occurs.
TEST_F(SyncerTest, UpdateWhereParentIsNotAFolder) {
  mock_server_->AddUpdateBookmark(1, 0, "B", 10, 10,
                                  foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(2, 1, "BookmarkParent", 10, 10,
                                   foreign_cache_guid(), "-2");
  // Used to cause a CHECK
  SyncShareNudge();
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry good_entry(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(good_entry.good());
    EXPECT_FALSE(good_entry.Get(IS_UNAPPLIED_UPDATE));
    Entry bad_parent(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.Get(IS_UNAPPLIED_UPDATE));
  }
}

const char kRootId[] = "0";

TEST_F(SyncerTest, DirectoryUpdateTest) {
  Id in_root_id = ids_.NewServerId();
  Id in_in_root_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(in_root_id, TestIdFactory::root(),
                                   "in_root_name", 2, 2,
                                   foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(in_in_root_id, in_root_id,
                                   "in_in_root_name", 3, 3,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry in_root(&trans, GET_BY_ID, in_root_id);
    ASSERT_TRUE(in_root.good());
    EXPECT_EQ("in_root_name", in_root.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(TestIdFactory::root(), in_root.Get(PARENT_ID));

    Entry in_in_root(&trans, GET_BY_ID, in_in_root_id);
    ASSERT_TRUE(in_in_root.good());
    EXPECT_EQ("in_in_root_name", in_in_root.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(in_root_id, in_in_root.Get(PARENT_ID));
  }
}

TEST_F(SyncerTest, DirectoryCommitTest) {
  syncable::Id in_root_id, in_dir_id;
  int64 foo_metahandle;
  int64 bar_metahandle;

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, root_id_, "foo");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    in_root_id = parent.Get(syncable::ID);
    foo_metahandle = parent.Get(META_HANDLE);

    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.Get(ID), "bar");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    bar_metahandle = child.Get(META_HANDLE);
    in_dir_id = parent.Get(syncable::ID);
  }
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry fail_by_old_id_entry(&trans, GET_BY_ID, in_root_id);
    ASSERT_FALSE(fail_by_old_id_entry.good());

    Entry foo_entry(&trans, GET_BY_HANDLE, foo_metahandle);
    ASSERT_TRUE(foo_entry.good());
    EXPECT_EQ("foo", foo_entry.Get(NON_UNIQUE_NAME));
    EXPECT_NE(foo_entry.Get(syncable::ID), in_root_id);

    Entry bar_entry(&trans, GET_BY_HANDLE, bar_metahandle);
    ASSERT_TRUE(bar_entry.good());
    EXPECT_EQ("bar", bar_entry.Get(NON_UNIQUE_NAME));
    EXPECT_NE(bar_entry.Get(syncable::ID), in_dir_id);
    EXPECT_EQ(foo_entry.Get(syncable::ID), bar_entry.Get(PARENT_ID));
  }
}

TEST_F(SyncerTest, TestClientCommandDuringUpdate) {
  using sync_pb::ClientCommand;

  ClientCommand* command = new ClientCommand();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetGUClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(8) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(800) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(3141) ==
              last_sessions_commit_delay_seconds_);

  command = new ClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetGUClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(180) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(190) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(2718) ==
              last_sessions_commit_delay_seconds_);
}

TEST_F(SyncerTest, TestClientCommandDuringCommit) {
  using sync_pb::ClientCommand;

  ClientCommand* command = new ClientCommand();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  CreateUnsyncedDirectory("X", "id_X");
  mock_server_->SetCommitClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(8) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(800) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(3141) ==
              last_sessions_commit_delay_seconds_);

  command = new ClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  CreateUnsyncedDirectory("Y", "id_Y");
  mock_server_->SetCommitClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(180) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(190) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(2718) ==
              last_sessions_commit_delay_seconds_);
}

TEST_F(SyncerTest, EnsureWeSendUpOldParent) {
  syncable::Id folder_one_id = ids_.FromNumber(1);
  syncable::Id folder_two_id = ids_.FromNumber(2);

  mock_server_->AddUpdateDirectory(folder_one_id, TestIdFactory::root(),
      "folder_one", 1, 1, foreign_cache_guid(), "-1");
  mock_server_->AddUpdateDirectory(folder_two_id, TestIdFactory::root(),
      "folder_two", 1, 1, foreign_cache_guid(), "-2");
  SyncShareNudge();
  {
    // A moved entry should send an "old parent."
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_ID, folder_one_id);
    ASSERT_TRUE(entry.good());
    entry.Put(PARENT_ID, folder_two_id);
    entry.Put(IS_UNSYNCED, true);
    // A new entry should send no "old parent."
    MutableEntry create(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "new_folder");
    create.Put(IS_UNSYNCED, true);
    create.Put(SPECIFICS, DefaultBookmarkSpecifics());
  }
  SyncShareNudge();
  const sync_pb::CommitMessage& commit = mock_server_->last_sent_commit();
  ASSERT_EQ(2, commit.entries_size());
  EXPECT_TRUE(commit.entries(0).parent_id_string() == "2");
  EXPECT_TRUE(commit.entries(0).old_parent_id() == "0");
  EXPECT_FALSE(commit.entries(1).has_old_parent_id());
}

TEST_F(SyncerTest, Test64BitVersionSupport) {
  int64 really_big_int = std::numeric_limits<int64>::max() - 12;
  const string name("ringo's dang orang ran rings around my o-ring");
  int64 item_metahandle;

  // Try writing max int64 to the version fields of a meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), name);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::BASE_VERSION, really_big_int);
    entry.Put(syncable::SERVER_VERSION, really_big_int);
    entry.Put(syncable::ID, ids_.NewServerId());
    item_metahandle = entry.Get(META_HANDLE);
  }
  // Now read it back out and make sure the value is max int64.
  syncable::ReadTransaction rtrans(FROM_HERE, directory());
  Entry entry(&rtrans, syncable::GET_BY_HANDLE, item_metahandle);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(really_big_int == entry.Get(syncable::BASE_VERSION));
}

TEST_F(SyncerTest, TestSimpleUndelete) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  mock_server_->set_conflict_all_commits(true);
  // Let there be an entry from the server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  // Check it out and delete it.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    // Delete it locally.
    entry.Put(IS_DEL, true);
  }
  SyncShareNudge();
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
  SyncShareNudge();
  // Update from server confirming deletion.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 11,
                                  foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();
  // IS_DEL AND SERVER_IS_DEL now both true.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_TRUE(entry.Get(SERVER_IS_DEL));
  }
  // Undelete from server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
}

TEST_F(SyncerTest, TestUndeleteWithMissingDeleteUpdate) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  // Let there be a entry, from the server.
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  // Check it out and delete it.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    // Delete it locally.
    entry.Put(IS_DEL, true);
  }
  SyncShareNudge();
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
  SyncShareNudge();
  // Say we do not get an update from server confirming deletion. Undelete
  // from server
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
}

TEST_F(SyncerTest, TestUndeleteIgnoreCorrectlyUnappliedUpdate) {
  Id id1 = ids_.MakeServer("first"), id2 = ids_.MakeServer("second");
  Id root = TestIdFactory::root();
  // Duplicate! expect path clashing!
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id1, root, "foo", 1, 10,
                                  foreign_cache_guid(), "-1");
  mock_server_->AddUpdateBookmark(id2, root, "foo", 1, 10,
                                  foreign_cache_guid(), "-2");
  SyncShareNudge();
  mock_server_->AddUpdateBookmark(id2, root, "foo2", 2, 20,
                                  foreign_cache_guid(), "-2");
  SyncShareNudge();  // Now just don't explode.
}

TEST_F(SyncerTest, ClientTagServerCreatedUpdatesWork) {
  mock_server_->AddUpdateDirectory(1, 0, "permitem1", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");

  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.Get(IS_DEL));
    EXPECT_FALSE(perm_folder.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(perm_folder.Get(IS_UNSYNCED));
    EXPECT_EQ(perm_folder.Get(UNIQUE_CLIENT_TAG), "permfolder");
    EXPECT_EQ(perm_folder.Get(NON_UNIQUE_NAME), "permitem1");
  }

  mock_server_->AddUpdateDirectory(1, 0, "permitem_renamed", 10, 100,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.Get(IS_DEL));
    EXPECT_FALSE(perm_folder.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(perm_folder.Get(IS_UNSYNCED));
    EXPECT_EQ(perm_folder.Get(UNIQUE_CLIENT_TAG), "permfolder");
    EXPECT_EQ(perm_folder.Get(NON_UNIQUE_NAME), "permitem_renamed");
  }
}

TEST_F(SyncerTest, ClientTagIllegalUpdateIgnored) {
  mock_server_->AddUpdateDirectory(1, 0, "permitem1", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");

  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(perm_folder.Get(IS_UNSYNCED));
    EXPECT_EQ(perm_folder.Get(UNIQUE_CLIENT_TAG), "permfolder");
    EXPECT_TRUE(perm_folder.Get(NON_UNIQUE_NAME) == "permitem1");
    EXPECT_TRUE(perm_folder.Get(ID).ServerKnows());
  }

  mock_server_->AddUpdateDirectory(1, 0, "permitem_renamed", 10, 100,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("wrongtag");
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // This update is rejected because it has the same ID, but a
    // different tag than one that is already on the client.
    // The client has a ServerKnows ID, which cannot be overwritten.
    Entry rejected_update(&trans, GET_BY_CLIENT_TAG, "wrongtag");
    EXPECT_FALSE(rejected_update.good());

    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(perm_folder.Get(IS_UNSYNCED));
    EXPECT_EQ(perm_folder.Get(NON_UNIQUE_NAME), "permitem1");
  }
}

TEST_F(SyncerTest, ClientTagUncommittedTagMatchesUpdate) {
  int64 original_metahandle = 0;

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(
        &trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    pref.Put(UNIQUE_CLIENT_TAG, "tag");
    pref.Put(IS_UNSYNCED, true);
    EXPECT_FALSE(pref.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(pref.Get(ID).ServerKnows());
    original_metahandle = pref.Get(META_HANDLE);
  }

  syncable::Id server_id = TestIdFactory::MakeServer("id");
  mock_server_->AddUpdatePref(server_id.GetServerId(),
                              ids_.root().GetServerId(),
                              "tag", 10, 100);
  mock_server_->set_conflict_all_commits(true);

  SyncShareNudge();
  // This should cause client tag reunion, preserving the metahandle.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    EXPECT_FALSE(pref.Get(IS_DEL));
    EXPECT_FALSE(pref.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(pref.Get(IS_UNSYNCED));
    EXPECT_EQ(10, pref.Get(BASE_VERSION));
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.Get(META_HANDLE));
    EXPECT_EQ("tag", pref.Get(UNIQUE_CLIENT_TAG));
    EXPECT_TRUE(pref.Get(ID).ServerKnows());
  }

  mock_server_->set_conflict_all_commits(false);
  SyncShareNudge();

  // The resolved entry ought to commit cleanly.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    EXPECT_FALSE(pref.Get(IS_DEL));
    EXPECT_FALSE(pref.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(pref.Get(IS_UNSYNCED));
    EXPECT_TRUE(10 < pref.Get(BASE_VERSION));
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.Get(META_HANDLE));
    EXPECT_EQ("tag", pref.Get(UNIQUE_CLIENT_TAG));
    EXPECT_TRUE(pref.Get(ID).ServerKnows());
  }
}

TEST_F(SyncerTest, ClientTagConflictWithDeletedLocalEntry) {
  {
    // Create a deleted local entry with a unique client tag.
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(
        &trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    ASSERT_FALSE(pref.Get(ID).ServerKnows());
    pref.Put(UNIQUE_CLIENT_TAG, "tag");
    pref.Put(IS_UNSYNCED, true);

    // Note: IS_DEL && !ServerKnows() will clear the UNSYNCED bit.
    // (We never attempt to commit server-unknown deleted items, so this
    // helps us clean up those entries).
    pref.Put(IS_DEL, true);
  }

  // Prepare an update with the same unique client tag.
  syncable::Id server_id = TestIdFactory::MakeServer("id");
  mock_server_->AddUpdatePref(server_id.GetServerId(),
                              ids_.root().GetServerId(),
                              "tag", 10, 100);

  SyncShareNudge();
  // The local entry will be overwritten.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    ASSERT_TRUE(pref.Get(ID).ServerKnows());
    EXPECT_FALSE(pref.Get(IS_DEL));
    EXPECT_FALSE(pref.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(pref.Get(IS_UNSYNCED));
    EXPECT_EQ(pref.Get(BASE_VERSION), 10);
    EXPECT_EQ(pref.Get(UNIQUE_CLIENT_TAG), "tag");
  }
}

TEST_F(SyncerTest, ClientTagUpdateClashesWithLocalEntry) {
  // This test is written assuming that ID comparison
  // will work out in a particular way.
  EXPECT_TRUE(ids_.FromNumber(1) < ids_.FromNumber(2));
  EXPECT_TRUE(ids_.FromNumber(3) < ids_.FromNumber(4));

  syncable::Id id1 = TestIdFactory::MakeServer("1");
  mock_server_->AddUpdatePref(id1.GetServerId(), ids_.root().GetServerId(),
                              "tag1", 10, 100);

  syncable::Id id4 = TestIdFactory::MakeServer("4");
  mock_server_->AddUpdatePref(id4.GetServerId(), ids_.root().GetServerId(),
                              "tag2", 11, 110);

  mock_server_->set_conflict_all_commits(true);

  SyncShareNudge();
  int64 tag1_metahandle = syncable::kInvalidMetaHandle;
  int64 tag2_metahandle = syncable::kInvalidMetaHandle;
  // This should cause client tag overwrite.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag1(&trans, GET_BY_CLIENT_TAG, "tag1");
    ASSERT_TRUE(tag1.good());
    ASSERT_TRUE(tag1.Get(ID).ServerKnows());
    ASSERT_TRUE(id1 == tag1.Get(ID));
    EXPECT_FALSE(tag1.Get(IS_DEL));
    EXPECT_FALSE(tag1.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag1.Get(IS_UNSYNCED));
    EXPECT_EQ(10, tag1.Get(BASE_VERSION));
    EXPECT_EQ("tag1", tag1.Get(UNIQUE_CLIENT_TAG));
    tag1_metahandle = tag1.Get(META_HANDLE);

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.Get(ID).ServerKnows());
    ASSERT_TRUE(id4 == tag2.Get(ID));
    EXPECT_FALSE(tag2.Get(IS_DEL));
    EXPECT_FALSE(tag2.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag2.Get(IS_UNSYNCED));
    EXPECT_EQ(11, tag2.Get(BASE_VERSION));
    EXPECT_EQ("tag2", tag2.Get(UNIQUE_CLIENT_TAG));
    tag2_metahandle = tag2.Get(META_HANDLE);

    syncable::Directory::ChildHandles children;
    directory()->GetChildHandlesById(&trans, trans.root_id(), &children);
    ASSERT_EQ(2U, children.size());
  }

  syncable::Id id2 = TestIdFactory::MakeServer("2");
  mock_server_->AddUpdatePref(id2.GetServerId(), ids_.root().GetServerId(),
                              "tag1", 12, 120);
  syncable::Id id3 = TestIdFactory::MakeServer("3");
  mock_server_->AddUpdatePref(id3.GetServerId(), ids_.root().GetServerId(),
                              "tag2", 13, 130);
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag1(&trans, GET_BY_CLIENT_TAG, "tag1");
    ASSERT_TRUE(tag1.good());
    ASSERT_TRUE(tag1.Get(ID).ServerKnows());
    ASSERT_EQ(id1, tag1.Get(ID))
        << "ID 1 should be kept, since it was less than ID 2.";
    EXPECT_FALSE(tag1.Get(IS_DEL));
    EXPECT_FALSE(tag1.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag1.Get(IS_UNSYNCED));
    EXPECT_EQ(10, tag1.Get(BASE_VERSION));
    EXPECT_EQ("tag1", tag1.Get(UNIQUE_CLIENT_TAG));
    EXPECT_EQ(tag1_metahandle, tag1.Get(META_HANDLE));

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.Get(ID).ServerKnows());
    ASSERT_EQ(id3, tag2.Get(ID))
        << "ID 3 should be kept, since it was less than ID 4.";
    EXPECT_FALSE(tag2.Get(IS_DEL));
    EXPECT_FALSE(tag2.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag2.Get(IS_UNSYNCED));
    EXPECT_EQ(13, tag2.Get(BASE_VERSION));
    EXPECT_EQ("tag2", tag2.Get(UNIQUE_CLIENT_TAG));
    EXPECT_EQ(tag2_metahandle, tag2.Get(META_HANDLE));

    syncable::Directory::ChildHandles children;
    directory()->GetChildHandlesById(&trans, trans.root_id(), &children);
    ASSERT_EQ(2U, children.size());
  }
}

TEST_F(SyncerTest, ClientTagClashWithinBatchOfUpdates) {
  // This test is written assuming that ID comparison
  // will work out in a particular way.
  EXPECT_TRUE(ids_.FromNumber(1) < ids_.FromNumber(4));
  EXPECT_TRUE(ids_.FromNumber(201) < ids_.FromNumber(205));

  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(1).GetServerId(),
                              ids_.root().GetServerId(), "tag a", 1, 10);
  mock_server_->AddUpdatePref(ids_.FromNumber(2).GetServerId(),
                              ids_.root().GetServerId(), "tag a", 11, 110);
  mock_server_->AddUpdatePref(ids_.FromNumber(3).GetServerId(),
                              ids_.root().GetServerId(), "tag a", 12, 120);
  mock_server_->AddUpdatePref(ids_.FromNumber(4).GetServerId(),
                              ids_.root().GetServerId(), "tag a", 13, 130);

  mock_server_->AddUpdatePref(ids_.FromNumber(105).GetServerId(),
                              ids_.root().GetServerId(), "tag b", 14, 140);
  mock_server_->AddUpdatePref(ids_.FromNumber(102).GetServerId(),
                              ids_.root().GetServerId(), "tag b", 15, 150);
  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(101).GetServerId(),
                              ids_.root().GetServerId(), "tag b", 16, 160);
  mock_server_->AddUpdatePref(ids_.FromNumber(104).GetServerId(),
                              ids_.root().GetServerId(), "tag b", 17, 170);

  mock_server_->AddUpdatePref(ids_.FromNumber(205).GetServerId(),
                              ids_.root().GetServerId(), "tag c", 18, 180);
  mock_server_->AddUpdatePref(ids_.FromNumber(202).GetServerId(),
                              ids_.root().GetServerId(), "tag c", 19, 190);
  mock_server_->AddUpdatePref(ids_.FromNumber(204).GetServerId(),
                              ids_.root().GetServerId(), "tag c", 20, 200);
  // Least ID: winner.
  mock_server_->AddUpdatePref(ids_.FromNumber(201).GetServerId(),
                              ids_.root().GetServerId(), "tag c", 21, 210);

  mock_server_->set_conflict_all_commits(true);

  SyncShareNudge();
  // This should cause client tag overwrite.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry tag_a(&trans, GET_BY_CLIENT_TAG, "tag a");
    ASSERT_TRUE(tag_a.good());
    EXPECT_TRUE(tag_a.Get(ID).ServerKnows());
    EXPECT_EQ(ids_.FromNumber(1), tag_a.Get(ID));
    EXPECT_FALSE(tag_a.Get(IS_DEL));
    EXPECT_FALSE(tag_a.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag_a.Get(IS_UNSYNCED));
    EXPECT_EQ(1, tag_a.Get(BASE_VERSION));
    EXPECT_EQ("tag a", tag_a.Get(UNIQUE_CLIENT_TAG));

    Entry tag_b(&trans, GET_BY_CLIENT_TAG, "tag b");
    ASSERT_TRUE(tag_b.good());
    EXPECT_TRUE(tag_b.Get(ID).ServerKnows());
    EXPECT_EQ(ids_.FromNumber(101), tag_b.Get(ID));
    EXPECT_FALSE(tag_b.Get(IS_DEL));
    EXPECT_FALSE(tag_b.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag_b.Get(IS_UNSYNCED));
    EXPECT_EQ(16, tag_b.Get(BASE_VERSION));
    EXPECT_EQ("tag b", tag_b.Get(UNIQUE_CLIENT_TAG));

    Entry tag_c(&trans, GET_BY_CLIENT_TAG, "tag c");
    ASSERT_TRUE(tag_c.good());
    EXPECT_TRUE(tag_c.Get(ID).ServerKnows());
    EXPECT_EQ(ids_.FromNumber(201), tag_c.Get(ID));
    EXPECT_FALSE(tag_c.Get(IS_DEL));
    EXPECT_FALSE(tag_c.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(tag_c.Get(IS_UNSYNCED));
    EXPECT_EQ(21, tag_c.Get(BASE_VERSION));
    EXPECT_EQ("tag c", tag_c.Get(UNIQUE_CLIENT_TAG));

    syncable::Directory::ChildHandles children;
    directory()->GetChildHandlesById(&trans, trans.root_id(), &children);
    ASSERT_EQ(3U, children.size());
  }
}

TEST_F(SyncerTest, UniqueServerTagUpdates) {
  // As a hurdle, introduce an item whose name is the same as the tag value
  // we'll use later.
  int64 hurdle_handle = CreateUnsyncedDirectory("bob", "id_bob");
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(UNIQUE_SERVER_TAG).empty());
    ASSERT_TRUE(hurdle.Get(NON_UNIQUE_NAME) == "bob");

    // Try to lookup by the tagname.  These should fail.
    Entry tag_alpha(&trans, GET_BY_SERVER_TAG, "alpha");
    EXPECT_FALSE(tag_alpha.good());
    Entry tag_bob(&trans, GET_BY_SERVER_TAG, "bob");
    EXPECT_FALSE(tag_bob.good());
  }

  // Now download some tagged items as updates.
  mock_server_->AddUpdateDirectory(1, 0, "update1", 1, 10, "", "");
  mock_server_->SetLastUpdateServerTag("alpha");
  mock_server_->AddUpdateDirectory(2, 0, "update2", 2, 20, "", "");
  mock_server_->SetLastUpdateServerTag("bob");
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // The new items should be applied as new entries, and we should be able
    // to look them up by their tag values.
    Entry tag_alpha(&trans, GET_BY_SERVER_TAG, "alpha");
    ASSERT_TRUE(tag_alpha.good());
    ASSERT_TRUE(!tag_alpha.Get(IS_DEL));
    ASSERT_TRUE(tag_alpha.Get(UNIQUE_SERVER_TAG) == "alpha");
    ASSERT_TRUE(tag_alpha.Get(NON_UNIQUE_NAME) == "update1");
    Entry tag_bob(&trans, GET_BY_SERVER_TAG, "bob");
    ASSERT_TRUE(tag_bob.good());
    ASSERT_TRUE(!tag_bob.Get(IS_DEL));
    ASSERT_TRUE(tag_bob.Get(UNIQUE_SERVER_TAG) == "bob");
    ASSERT_TRUE(tag_bob.Get(NON_UNIQUE_NAME) == "update2");
    // The old item should be unchanged.
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(UNIQUE_SERVER_TAG).empty());
    ASSERT_TRUE(hurdle.Get(NON_UNIQUE_NAME) == "bob");
  }
}

TEST_F(SyncerTest, GetUpdatesSetsRequestedTypes) {
  // The expectations of this test happen in the MockConnectionManager's
  // GetUpdates handler.  EnableDatatype sets the expectation value from our
  // set of enabled/disabled datatypes.
  EnableDatatype(BOOKMARKS);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  EnableDatatype(AUTOFILL);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  EnableDatatype(PREFERENCES);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(BOOKMARKS);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(AUTOFILL);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  DisableDatatype(PREFERENCES);
  EnableDatatype(AUTOFILL);
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
}

// A typical scenario: server and client each have one update for the other.
// This is the "happy path" alternative to UpdateFailsThenDontCommit.
TEST_F(SyncerTest, UpdateThenCommit) {
  syncable::Id to_receive = ids_.NewServerId();
  syncable::Id to_commit = ids_.NewLocalId();

  mock_server_->AddUpdateDirectory(to_receive, ids_.root(), "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  int64 commit_handle = CreateUnsyncedDirectory("y", to_commit);
  SyncShareNudge();

  // The sync cycle should have included a GetUpdate, then a commit.  By the
  // time the commit happened, we should have known for sure that there were no
  // hierarchy conflicts, and reported this fact to the server.
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  VerifyNoHierarchyConflictsReported(mock_server_->last_request());

  syncable::ReadTransaction trans(FROM_HERE, directory());

  Entry received(&trans, GET_BY_ID, to_receive);
  ASSERT_TRUE(received.good());
  EXPECT_FALSE(received.Get(IS_UNSYNCED));
  EXPECT_FALSE(received.Get(IS_UNAPPLIED_UPDATE));

  Entry committed(&trans, GET_BY_HANDLE, commit_handle);
  ASSERT_TRUE(committed.good());
  EXPECT_FALSE(committed.Get(IS_UNSYNCED));
  EXPECT_FALSE(committed.Get(IS_UNAPPLIED_UPDATE));
}

// Same as above, but this time we fail to download updates.
// We should not attempt to commit anything unless we successfully downloaded
// updates, otherwise we risk causing a server-side conflict.
TEST_F(SyncerTest, UpdateFailsThenDontCommit) {
  syncable::Id to_receive = ids_.NewServerId();
  syncable::Id to_commit = ids_.NewLocalId();

  mock_server_->AddUpdateDirectory(to_receive, ids_.root(), "x", 1, 10,
                                   foreign_cache_guid(), "-1");
  int64 commit_handle = CreateUnsyncedDirectory("y", to_commit);
  mock_server_->FailNextPostBufferToPathCall();
  SyncShareNudge();

  syncable::ReadTransaction trans(FROM_HERE, directory());

  // We did not receive this update.
  Entry received(&trans, GET_BY_ID, to_receive);
  ASSERT_FALSE(received.good());

  // And our local update remains unapplied.
  Entry committed(&trans, GET_BY_HANDLE, commit_handle);
  ASSERT_TRUE(committed.good());
  EXPECT_TRUE(committed.Get(IS_UNSYNCED));
  EXPECT_FALSE(committed.Get(IS_UNAPPLIED_UPDATE));

  // Inform the Mock we won't be fetching all updates.
  mock_server_->ClearUpdatesQueue();
}

// Downloads two updates and applies them successfully.
// This is the "happy path" alternative to ConfigureFailsDontApplyUpdates.
TEST_F(SyncerTest, ConfigureDownloadsTwoBatchesSuccess) {
  syncable::Id node1 = ids_.NewServerId();
  syncable::Id node2 = ids_.NewServerId();

  // Construct the first GetUpdates response.
  mock_server_->AddUpdateDirectory(node1, ids_.root(), "one", 1, 10,
                                   foreign_cache_guid(), "-2");
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Construct the second GetUpdates response.
  mock_server_->AddUpdateDirectory(node2, ids_.root(), "two", 1, 20,
                                   foreign_cache_guid(), "-2");

  SyncShareConfigure();

  syncable::ReadTransaction trans(FROM_HERE, directory());
  // Both nodes should be downloaded and applied.

  Entry n1(&trans, GET_BY_ID, node1);
  ASSERT_TRUE(n1.good());
  EXPECT_FALSE(n1.Get(IS_UNAPPLIED_UPDATE));

  Entry n2(&trans, GET_BY_ID, node2);
  ASSERT_TRUE(n2.good());
  EXPECT_FALSE(n2.Get(IS_UNAPPLIED_UPDATE));
}

// Same as the above case, but this time the second batch fails to download.
TEST_F(SyncerTest, ConfigureFailsDontApplyUpdates) {
  syncable::Id node1 = ids_.NewServerId();
  syncable::Id node2 = ids_.NewServerId();

  // The scenario: we have two batches of updates with one update each.  A
  // normal confgure step would download all the updates one batch at a time and
  // apply them.  This configure will succeed in downloading the first batch
  // then fail when downloading the second.
  mock_server_->FailNthPostBufferToPathCall(2);

  // Construct the first GetUpdates response.
  mock_server_->AddUpdateDirectory(node1, ids_.root(), "one", 1, 10,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetChangesRemaining(1);
  mock_server_->NextUpdateBatch();

  // Consutrct the second GetUpdates response.
  mock_server_->AddUpdateDirectory(node2, ids_.root(), "two", 1, 20,
                                   foreign_cache_guid(), "-2");

  SyncShareConfigure();

  syncable::ReadTransaction trans(FROM_HERE, directory());

  // The first node was downloaded, but not applied.
  Entry n1(&trans, GET_BY_ID, node1);
  ASSERT_TRUE(n1.good());
  EXPECT_TRUE(n1.Get(IS_UNAPPLIED_UPDATE));

  // The second node was not downloaded.
  Entry n2(&trans, GET_BY_ID, node2);
  EXPECT_FALSE(n2.good());

  // One update remains undownloaded.
  mock_server_->ClearUpdatesQueue();
}

TEST_F(SyncerTest, GetKeySuccess) {
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->NeedKeystoreKey(&rtrans));
  }

  SyncShareConfigure();

  EXPECT_EQ(session_->status_controller().last_get_key_result(), SYNCER_OK);
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    EXPECT_FALSE(directory()->GetNigoriHandler()->NeedKeystoreKey(&rtrans));
  }
}

TEST_F(SyncerTest, GetKeyEmpty) {
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->NeedKeystoreKey(&rtrans));
  }

  mock_server_->SetKeystoreKey("");
  SyncShareConfigure();

  EXPECT_NE(session_->status_controller().last_get_key_result(), SYNCER_OK);
  {
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    EXPECT_TRUE(directory()->GetNigoriHandler()->NeedKeystoreKey(&rtrans));
  }
}

// Test what happens if a client deletes, then recreates, an object very
// quickly.  It is possible that the deletion gets sent as a commit, and
// the undelete happens during the commit request.  The principle here
// is that with a single committing client, conflicts should never
// be encountered, and a client encountering its past actions during
// getupdates should never feed back to override later actions.
//
// In cases of ordering A-F below, the outcome should be the same.
//   Exercised by UndeleteDuringCommit:
//     A. Delete - commit - undelete - commitresponse.
//     B. Delete - commit - undelete - commitresponse - getupdates.
//   Exercised by UndeleteBeforeCommit:
//     C. Delete - undelete - commit - commitresponse.
//     D. Delete - undelete - commit - commitresponse - getupdates.
//   Exercised by UndeleteAfterCommit:
//     E. Delete - commit - commitresponse - undelete - commit
//        - commitresponse.
//     F. Delete - commit - commitresponse - undelete - commit -
//        - commitresponse - getupdates.
class SyncerUndeletionTest : public SyncerTest {
 public:
  SyncerUndeletionTest()
      : client_tag_("foobar"),
        metahandle_(syncable::kInvalidMetaHandle) {
  }

  void Create() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry perm_folder(
        &trans, CREATE, BOOKMARKS, ids_.root(), "clientname");
    ASSERT_TRUE(perm_folder.good());
    perm_folder.Put(UNIQUE_CLIENT_TAG, client_tag_);
    perm_folder.Put(IS_UNSYNCED, true);
    perm_folder.Put(SYNCING, false);
    perm_folder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    EXPECT_FALSE(perm_folder.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(perm_folder.Get(ID).ServerKnows());
    metahandle_ = perm_folder.Get(META_HANDLE);
    local_id_ = perm_folder.Get(ID);
  }

  void Delete() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.Get(META_HANDLE));
    entry.Put(IS_DEL, true);
    entry.Put(IS_UNSYNCED, true);
    entry.Put(SYNCING, false);
  }

  void Undelete() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.Get(META_HANDLE));
    EXPECT_TRUE(entry.Get(IS_DEL));
    entry.Put(IS_DEL, false);
    entry.Put(IS_UNSYNCED, true);
    entry.Put(SYNCING, false);
  }

  int64 GetMetahandleOfTag() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    EXPECT_TRUE(entry.good());
    if (!entry.good()) {
      return syncable::kInvalidMetaHandle;
    }
    return entry.Get(META_HANDLE);
  }

  void ExpectUnsyncedCreation() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_FALSE(Get(metahandle_, IS_DEL));
    EXPECT_FALSE(Get(metahandle_, SERVER_IS_DEL));  // Never been committed.
    EXPECT_GE(0, Get(metahandle_, BASE_VERSION));
    EXPECT_TRUE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
  }

  void ExpectUnsyncedUndeletion() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_FALSE(Get(metahandle_, IS_DEL));
    EXPECT_TRUE(Get(metahandle_, SERVER_IS_DEL));
    EXPECT_EQ(0, Get(metahandle_, BASE_VERSION));
    EXPECT_TRUE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(Get(metahandle_, ID).ServerKnows());
  }

  void ExpectUnsyncedEdit() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_FALSE(Get(metahandle_, IS_DEL));
    EXPECT_FALSE(Get(metahandle_, SERVER_IS_DEL));
    EXPECT_LT(0, Get(metahandle_, BASE_VERSION));
    EXPECT_TRUE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(Get(metahandle_, ID).ServerKnows());
  }

  void ExpectUnsyncedDeletion() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_TRUE(Get(metahandle_, IS_DEL));
    EXPECT_FALSE(Get(metahandle_, SERVER_IS_DEL));
    EXPECT_TRUE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
    EXPECT_LT(0, Get(metahandle_, BASE_VERSION));
    EXPECT_LT(0, Get(metahandle_, SERVER_VERSION));
  }

  void ExpectSyncedAndCreated() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_FALSE(Get(metahandle_, IS_DEL));
    EXPECT_FALSE(Get(metahandle_, SERVER_IS_DEL));
    EXPECT_LT(0, Get(metahandle_, BASE_VERSION));
    EXPECT_EQ(Get(metahandle_, BASE_VERSION), Get(metahandle_, SERVER_VERSION));
    EXPECT_FALSE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
  }

  void ExpectSyncedAndDeleted() {
    EXPECT_EQ(metahandle_, GetMetahandleOfTag());
    EXPECT_TRUE(Get(metahandle_, IS_DEL));
    EXPECT_TRUE(Get(metahandle_, SERVER_IS_DEL));
    EXPECT_FALSE(Get(metahandle_, IS_UNSYNCED));
    EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));
    EXPECT_GE(0, Get(metahandle_, BASE_VERSION));
    EXPECT_GE(0, Get(metahandle_, SERVER_VERSION));
  }

 protected:
  const std::string client_tag_;
  syncable::Id local_id_;
  int64 metahandle_;
};

TEST_F(SyncerUndeletionTest, UndeleteDuringCommit) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete, begin committing the delete, then undelete while committing.
  Delete();
  ExpectUnsyncedDeletion();
  mock_server_->SetMidCommitCallback(
      base::Bind(&SyncerUndeletionTest::Undelete, base::Unretained(this)));
  SyncShareNudge();

  // We will continue to commit until all nodes are synced, so we expect
  // that both the delete and following undelete were committed.  We haven't
  // downloaded any updates, though, so the SERVER fields will be the same
  // as they were at the start of the cycle.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());

  // Server fields lag behind.
  EXPECT_FALSE(Get(metahandle_, SERVER_IS_DEL));

  // We have committed the second (undelete) update.
  EXPECT_FALSE(Get(metahandle_, IS_DEL));
  EXPECT_FALSE(Get(metahandle_, IS_UNSYNCED));
  EXPECT_FALSE(Get(metahandle_, IS_UNAPPLIED_UPDATE));

  // Now, encounter a GetUpdates corresponding to the deletion from
  // the server.  The undeletion should prevail again and be committed.
  // None of this should trigger any conflict detection -- it is perfectly
  // normal to recieve updates from our own commits.
  mock_server_->SetMidCommitCallback(base::Closure());
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());

  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteBeforeCommit) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and undelete, then sync to pick up the result.
  Delete();
  ExpectUnsyncedDeletion();
  Undelete();
  ExpectUnsyncedEdit();  // Edit, not undelete: server thinks it exists.
  SyncShareNudge();

  // The item ought to have committed successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  EXPECT_EQ(2, Get(metahandle_, BASE_VERSION));

  // Now, encounter a GetUpdates corresponding to the just-committed
  // update.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterCommitButBeforeGetUpdates) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and commit.
  Delete();
  ExpectUnsyncedDeletion();
  SyncShareNudge();

  // The item ought to have committed successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Before the GetUpdates, the item is locally undeleted.
  Undelete();
  ExpectUnsyncedUndeletion();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  mock_server_->AddUpdateFromLastCommit();
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterDeleteAndGetUpdates) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Delete and commit.
  Delete();
  ExpectUnsyncedDeletion();
  SyncShareNudge();

  // The item ought to have committed successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  Should be consistent.
  mock_server_->AddUpdateFromLastCommit();
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // After the GetUpdates, the item is locally undeleted.
  Undelete();
  ExpectUnsyncedUndeletion();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

// Test processing of undeletion GetUpdateses.
TEST_F(SyncerUndeletionTest, UndeleteAfterOtherClientDeletes) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Add a delete from the server.
  sync_pb::SyncEntity* update1 = mock_server_->AddUpdateFromLastCommit();
  update1->set_originator_cache_guid(local_cache_guid());
  update1->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Some other client deletes the item.
  mock_server_->AddUpdateTombstone(Get(metahandle_, ID));
  SyncShareNudge();

  // The update ought to have applied successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Undelete it locally.
  Undelete();
  ExpectUnsyncedUndeletion();
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  sync_pb::SyncEntity* update2 = mock_server_->AddUpdateFromLastCommit();
  update2->set_originator_cache_guid(local_cache_guid());
  update2->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, UndeleteAfterOtherClientDeletesImmediately) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Some other client deletes the item before we get a chance
  // to GetUpdates our original request.
  mock_server_->AddUpdateTombstone(Get(metahandle_, ID));
  SyncShareNudge();

  // The update ought to have applied successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Undelete it locally.
  Undelete();
  ExpectUnsyncedUndeletion();
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.  The undeletion should prevail.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
}

TEST_F(SyncerUndeletionTest, OtherClientUndeletes) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Get the updates of our just-committed entry.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // We delete the item.
  Delete();
  ExpectUnsyncedDeletion();
  SyncShareNudge();

  // The update ought to have applied successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Now, encounter a GetUpdates corresponding to the just-committed
  // deletion update.
  mock_server_->AddUpdateFromLastCommit();
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Some other client undeletes the item.
  mock_server_->AddUpdateBookmark(Get(metahandle_, ID),
                                  Get(metahandle_, PARENT_ID),
                                  "Thadeusz", 100, 1000,
                                  local_cache_guid(), local_id_.GetServerId());
  mock_server_->SetLastUpdateClientTag(client_tag_);
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  EXPECT_EQ("Thadeusz", Get(metahandle_, NON_UNIQUE_NAME));
}

TEST_F(SyncerUndeletionTest, OtherClientUndeletesImmediately) {
  Create();
  ExpectUnsyncedCreation();
  SyncShareNudge();

  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // Get the updates of our just-committed entry.
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id_.GetServerId());
  SyncShareNudge();
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  ExpectSyncedAndCreated();

  // We delete the item.
  Delete();
  ExpectUnsyncedDeletion();
  SyncShareNudge();

  // The update ought to have applied successfully.
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndDeleted();

  // Some other client undeletes before we see the update from our
  // commit.
  mock_server_->AddUpdateBookmark(Get(metahandle_, ID),
                                  Get(metahandle_, PARENT_ID),
                                  "Thadeusz", 100, 1000,
                                  local_cache_guid(), local_id_.GetServerId());
  mock_server_->SetLastUpdateClientTag(client_tag_);
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  EXPECT_EQ("Thadeusz", Get(metahandle_, NON_UNIQUE_NAME));
}

// A group of tests exercising the syncer's handling of sibling ordering, as
// represented in the sync protocol.
class SyncerPositionUpdateTest : public SyncerTest {
 public:
  SyncerPositionUpdateTest() : next_update_id_(1), next_revision_(1) {}

 protected:
  void ExpectLocalItemsInServerOrder() {
    if (position_map_.empty())
      return;

    syncable::ReadTransaction trans(FROM_HERE, directory());

    Id prev_id;
    DCHECK(prev_id.IsRoot());
    PosMap::iterator next = position_map_.begin();
    for (PosMap::iterator i = next++; i != position_map_.end(); ++i) {
      Id id = i->second;
      Entry entry_with_id(&trans, GET_BY_ID, id);
      EXPECT_TRUE(entry_with_id.good());
      EXPECT_EQ(prev_id, entry_with_id.GetPredecessorId());
      EXPECT_EQ(
          i->first,
          NodeOrdinalToInt64(entry_with_id.Get(SERVER_ORDINAL_IN_PARENT)));
      if (next == position_map_.end()) {
        EXPECT_EQ(Id(), entry_with_id.GetSuccessorId());
      } else {
        EXPECT_EQ(next->second, entry_with_id.GetSuccessorId());
        next++;
      }
      prev_id = id;
    }
  }

  void AddRootItemWithPosition(int64 position) {
    string id = string("ServerId") + base::Int64ToString(next_update_id_++);
    string name = "my name is my id -- " + id;
    int revision = next_revision_++;
    mock_server_->AddUpdateDirectory(id, kRootId, name, revision, revision,
                                     foreign_cache_guid(),
                                     ids_.NewLocalId().GetServerId());
    mock_server_->SetLastUpdatePosition(position);
    position_map_.insert(
        PosMap::value_type(position, Id::CreateFromServerId(id)));
  }
 private:
  typedef multimap<int64, Id> PosMap;
  PosMap position_map_;
  int next_update_id_;
  int next_revision_;
  DISALLOW_COPY_AND_ASSIGN(SyncerPositionUpdateTest);
};

TEST_F(SyncerPositionUpdateTest, InOrderPositive) {
  // Add a bunch of items in increasing order, starting with just positive
  // position values.
  AddRootItemWithPosition(100);
  AddRootItemWithPosition(199);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(400);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, InOrderNegative) {
  // Test negative position values, but in increasing order.
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(-201);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(100);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, ReverseOrder) {
  // Test when items are sent in the reverse order.
  AddRootItemWithPosition(400);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(100);
  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(-201);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(-400);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, RandomOrderInBatches) {
  // Mix it all up, interleaving position values, and try multiple batches of
  // updates.
  AddRootItemWithPosition(400);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(100);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(-201);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-144);

  SyncShareNudge();
  ExpectLocalItemsInServerOrder();
}

class SyncerPositionTiebreakingTest : public SyncerTest {
 public:
  SyncerPositionTiebreakingTest()
      : low_id_(Id::CreateFromServerId("A")),
        mid_id_(Id::CreateFromServerId("M")),
        high_id_(Id::CreateFromServerId("Z")),
        next_revision_(1) {
    DCHECK(low_id_ < mid_id_);
    DCHECK(mid_id_ < high_id_);
    DCHECK(low_id_ < high_id_);
  }

  // Adds the item by its Id, using a constant value for the position
  // so that the syncer has to resolve the order some other way.
  void Add(const Id& id) {
    int revision = next_revision_++;
    mock_server_->AddUpdateDirectory(id.GetServerId(), kRootId,
        id.GetServerId(), revision, revision,
        foreign_cache_guid(), ids_.NewLocalId().GetServerId());
    // The update position doesn't vary.
    mock_server_->SetLastUpdatePosition(90210);
  }

  void ExpectLocalOrderIsByServerId() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Id null_id;
    Entry low(&trans, GET_BY_ID, low_id_);
    Entry mid(&trans, GET_BY_ID, mid_id_);
    Entry high(&trans, GET_BY_ID, high_id_);
    EXPECT_TRUE(low.good());
    EXPECT_TRUE(mid.good());
    EXPECT_TRUE(high.good());
    EXPECT_TRUE(low.GetPredecessorId() == null_id);
    EXPECT_TRUE(mid.GetPredecessorId() == low_id_);
    EXPECT_TRUE(high.GetPredecessorId() == mid_id_);
    EXPECT_TRUE(high.GetSuccessorId() == null_id);
    EXPECT_TRUE(mid.GetSuccessorId() == high_id_);
    EXPECT_TRUE(low.GetSuccessorId() == mid_id_);
  }

 protected:
  // When there's a tiebreak on the numeric position, it's supposed to be
  // broken by string comparison of the ids.  These ids are in increasing
  // order.
  const Id low_id_;
  const Id mid_id_;
  const Id high_id_;

 private:
  int next_revision_;
  DISALLOW_COPY_AND_ASSIGN(SyncerPositionTiebreakingTest);
};

TEST_F(SyncerPositionTiebreakingTest, LowMidHigh) {
  Add(low_id_);
  Add(mid_id_);
  Add(high_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, LowHighMid) {
  Add(low_id_);
  Add(high_id_);
  Add(mid_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighMidLow) {
  Add(high_id_);
  Add(mid_id_);
  Add(low_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighLowMid) {
  Add(high_id_);
  Add(low_id_);
  Add(mid_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidHighLow) {
  Add(mid_id_);
  Add(high_id_);
  Add(low_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidLowHigh) {
  Add(mid_id_);
  Add(low_id_);
  Add(high_id_);
  SyncShareNudge();
  ExpectLocalOrderIsByServerId();
}

enum {
  TEST_PARAM_BOOKMARK_ENABLE_BIT,
  TEST_PARAM_AUTOFILL_ENABLE_BIT,
  TEST_PARAM_BIT_COUNT
};

class MixedResult :
    public SyncerTest,
    public ::testing::WithParamInterface<int> {
 protected:
  bool ShouldFailBookmarkCommit() {
    return (GetParam() & (1 << TEST_PARAM_BOOKMARK_ENABLE_BIT)) == 0;
  }
  bool ShouldFailAutofillCommit() {
    return (GetParam() & (1 << TEST_PARAM_AUTOFILL_ENABLE_BIT)) == 0;
  }
};

INSTANTIATE_TEST_CASE_P(ExtensionsActivity,
                        MixedResult,
                        testing::Range(0, 1 << TEST_PARAM_BIT_COUNT));

TEST_P(MixedResult, ExtensionsActivity) {
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());

    MutableEntry pref(&wtrans, CREATE, PREFERENCES, wtrans.root_id(), "pref");
    ASSERT_TRUE(pref.good());
    pref.Put(syncable::IS_UNSYNCED, true);

    MutableEntry bookmark(
        &wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "bookmark");
    ASSERT_TRUE(bookmark.good());
    bookmark.Put(syncable::IS_UNSYNCED, true);

    if (ShouldFailBookmarkCommit()) {
      mock_server_->SetTransientErrorId(bookmark.Get(ID));
    }

    if (ShouldFailAutofillCommit()) {
      mock_server_->SetTransientErrorId(pref.Get(ID));
    }
  }


  // Put some extenions activity records into the monitor.
  {
    ExtensionsActivityMonitor::Records records;
    records["ABC"].extension_id = "ABC";
    records["ABC"].bookmark_write_count = 2049U;
    records["xyz"].extension_id = "xyz";
    records["xyz"].bookmark_write_count = 4U;
    context_->extensions_monitor()->PutRecords(records);
  }

  SyncShareNudge();

  ExtensionsActivityMonitor::Records final_monitor_records;
  context_->extensions_monitor()->GetAndClearRecords(&final_monitor_records);
  if (ShouldFailBookmarkCommit()) {
    ASSERT_EQ(2U, final_monitor_records.size())
        << "Should restore records after unsuccessful bookmark commit.";
    EXPECT_EQ("ABC", final_monitor_records["ABC"].extension_id);
    EXPECT_EQ("xyz", final_monitor_records["xyz"].extension_id);
    EXPECT_EQ(2049U, final_monitor_records["ABC"].bookmark_write_count);
    EXPECT_EQ(4U,    final_monitor_records["xyz"].bookmark_write_count);
  } else {
    EXPECT_TRUE(final_monitor_records.empty())
        << "Should not restore records after successful bookmark commit.";
  }
}

}  // namespace syncer
