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
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "sync/engine/get_commit_ids.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/commit_counters.h"
#include "sync/internal_api/public/sessions/status_counters.h"
#include "sync/internal_api/public/sessions/update_counters.h"
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
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "sync/test/fake_encryptor.h"
#include "sync/test/fake_sync_encryption_handler.h"
#include "sync/test/sessions/mock_debug_info_getter.h"
#include "sync/util/cryptographer.h"
#include "sync/util/extensions_activity.h"
#include "sync/util/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

using std::count;
using std::map;
using std::multimap;
using std::set;
using std::string;
using std::vector;

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

using syncable::CREATE;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::GET_BY_CLIENT_TAG;
using syncable::GET_BY_SERVER_TAG;
using syncable::GET_TYPE_ROOT;
using syncable::UNITTEST;

using sessions::MockDebugInfoGetter;
using sessions::StatusController;
using sessions::SyncSessionContext;
using sessions::SyncSession;

namespace {

// A helper to hold on to the counters emitted by the sync engine.
class TypeDebugInfoCache : public TypeDebugInfoObserver {
 public:
  TypeDebugInfoCache();
  virtual ~TypeDebugInfoCache();

  CommitCounters GetLatestCommitCounters(ModelType type) const;
  UpdateCounters GetLatestUpdateCounters(ModelType type) const;
  StatusCounters GetLatestStatusCounters(ModelType type) const;

  // TypeDebugInfoObserver implementation.
  virtual void OnCommitCountersUpdated(
      syncer::ModelType type,
      const CommitCounters& counters) OVERRIDE;
  virtual void OnUpdateCountersUpdated(
      syncer::ModelType type,
      const UpdateCounters& counters) OVERRIDE;
  virtual void OnStatusCountersUpdated(
      syncer::ModelType type,
      const StatusCounters& counters) OVERRIDE;

 private:
  std::map<ModelType, CommitCounters> commit_counters_map_;
  std::map<ModelType, UpdateCounters> update_counters_map_;
  std::map<ModelType, StatusCounters> status_counters_map_;
};

TypeDebugInfoCache::TypeDebugInfoCache() {}

TypeDebugInfoCache::~TypeDebugInfoCache() {}

CommitCounters TypeDebugInfoCache::GetLatestCommitCounters(
    ModelType type) const {
  std::map<ModelType, CommitCounters>::const_iterator it =
      commit_counters_map_.find(type);
  if (it == commit_counters_map_.end()) {
    return CommitCounters();
  } else {
    return it->second;
  }
}

UpdateCounters TypeDebugInfoCache::GetLatestUpdateCounters(
    ModelType type) const {
  std::map<ModelType, UpdateCounters>::const_iterator it =
      update_counters_map_.find(type);
  if (it == update_counters_map_.end()) {
    return UpdateCounters();
  } else {
    return it->second;
  }
}

StatusCounters TypeDebugInfoCache::GetLatestStatusCounters(
    ModelType type) const {
  std::map<ModelType, StatusCounters>::const_iterator it =
      status_counters_map_.find(type);
  if (it == status_counters_map_.end()) {
    return StatusCounters();
  } else {
    return it->second;
  }
}

void TypeDebugInfoCache::OnCommitCountersUpdated(
    syncer::ModelType type,
    const CommitCounters& counters) {
  commit_counters_map_[type] = counters;
}

void TypeDebugInfoCache::OnUpdateCountersUpdated(
    syncer::ModelType type,
    const UpdateCounters& counters) {
  update_counters_map_[type] = counters;
}

void TypeDebugInfoCache::OnStatusCountersUpdated(
    syncer::ModelType type,
    const StatusCounters& counters) {
  status_counters_map_[type] = counters;
}

} // namespace

class SyncerTest : public testing::Test,
                   public SyncSession::Delegate,
                   public SyncEngineEventListener {
 protected:
  SyncerTest()
      : extensions_activity_(new ExtensionsActivity),
        syncer_(NULL),
        saw_syncer_event_(false),
        last_client_invalidation_hint_buffer_size_(10) {
}

  // SyncSession::Delegate implementation.
  virtual void OnThrottled(const base::TimeDelta& throttle_duration) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual void OnTypesThrottled(
      ModelTypeSet types,
      const base::TimeDelta& throttle_duration) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsCurrentlyThrottled() OVERRIDE {
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
  virtual void OnReceivedClientInvalidationHintBufferSize(
      int size) OVERRIDE {
    last_client_invalidation_hint_buffer_size_ = size;
  }
  virtual void OnReceivedGuRetryDelay(const base::TimeDelta& delay) OVERRIDE {}
  virtual void OnReceivedMigrationRequest(ModelTypeSet types) OVERRIDE {}
  virtual void OnProtocolEvent(const ProtocolEvent& event) OVERRIDE {}
  virtual void OnSyncProtocolError(const SyncProtocolError& error) OVERRIDE {}

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    // We're just testing the sync engine here, so we shunt everything to
    // the SyncerThread.  Datatypes which aren't enabled aren't in the map.
    for (ModelTypeSet::Iterator it = enabled_datatypes_.First();
         it.Good(); it.Inc()) {
      (*out)[it.Get()] = GROUP_PASSIVE;
    }
  }

  virtual void OnSyncCycleEvent(const SyncCycleEvent& event) OVERRIDE {
    DVLOG(1) << "HandleSyncEngineEvent in unittest " << event.what_happened;
    // we only test for entry-specific events, not status changed ones.
    switch (event.what_happened) {
      case SyncCycleEvent::SYNC_CYCLE_BEGIN: // Fall through.
      case SyncCycleEvent::STATUS_CHANGED:
      case SyncCycleEvent::SYNC_CYCLE_ENDED:
        return;
      default:
        CHECK(false) << "Handling unknown error type in unit tests!!";
    }
    saw_syncer_event_ = true;
  }

  virtual void OnActionableError(const SyncProtocolError& error) OVERRIDE {}
  virtual void OnRetryTimeChanged(base::Time retry_time) OVERRIDE {}
  virtual void OnThrottledTypesChanged(ModelTypeSet throttled_types) OVERRIDE {}
  virtual void OnMigrationRequested(ModelTypeSet types) OVERRIDE {}

  void ResetSession() {
    session_.reset(SyncSession::Build(context_.get(), this));
  }

  void SyncShareNudge() {
    ResetSession();

    // Pretend we've seen a local change, to make the nudge_tracker look normal.
    nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));

    EXPECT_TRUE(
        syncer_->NormalSyncShare(
            context_->GetEnabledTypes(),
            nudge_tracker_,
            session_.get()));
  }

  void SyncShareConfigure() {
    ResetSession();
    EXPECT_TRUE(syncer_->ConfigureSyncShare(
            context_->GetEnabledTypes(),
            sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
            session_.get()));
  }

  virtual void SetUp() {
    dir_maker_.SetUp();
    mock_server_.reset(new MockConnectionManager(directory(),
                                                 &cancelation_signal_));
    debug_info_getter_.reset(new MockDebugInfoGetter);
    EnableDatatype(BOOKMARKS);
    EnableDatatype(NIGORI);
    EnableDatatype(PREFERENCES);
    EnableDatatype(NIGORI);
    workers_.push_back(scoped_refptr<ModelSafeWorker>(
        new FakeModelWorker(GROUP_PASSIVE)));
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(this);

    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    model_type_registry_.reset(
        new ModelTypeRegistry(workers_, directory(), &mock_nudge_handler_));
    model_type_registry_->RegisterDirectoryTypeDebugInfoObserver(
        &debug_info_cache_);

    context_.reset(
        new SyncSessionContext(
            mock_server_.get(), directory(),
            extensions_activity_,
            listeners, debug_info_getter_.get(),
            model_type_registry_.get(),
            true,  // enable keystore encryption
            false,  // force enable pre-commit GU avoidance experiment
            "fake_invalidator_client_id"));
    context_->SetRoutingInfo(routing_info);
    syncer_ = new Syncer(&cancelation_signal_);

    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Directory::Metahandles children;
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
    model_type_registry_->UnregisterDirectoryTypeDebugInfoObserver(
        &debug_info_cache_);
    mock_server_.reset();
    delete syncer_;
    syncer_ = NULL;
    dir_maker_.TearDown();
  }

  void WriteTestDataToEntry(WriteTransaction* trans, MutableEntry* entry) {
    EXPECT_FALSE(entry->GetIsDir());
    EXPECT_FALSE(entry->GetIsDel());
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_url("http://demo/");
    specifics.mutable_bookmark()->set_favicon("PNG");
    entry->PutSpecifics(specifics);
    entry->PutIsUnsynced(true);
  }
  void VerifyTestDataInEntry(BaseTransaction* trans, Entry* entry) {
    EXPECT_FALSE(entry->GetIsDir());
    EXPECT_FALSE(entry->GetIsDel());
    VerifyTestBookmarkDataInEntry(entry);
  }
  void VerifyTestBookmarkDataInEntry(Entry* entry) {
    const sync_pb::EntitySpecifics& specifics = entry->GetSpecifics();
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

        entry.PutId(test->id);
        if (test->id.ServerKnows()) {
          entry.PutBaseVersion(5);
          entry.PutServerVersion(5);
          entry.PutServerParentId(test->parent_id);
        }
        entry.PutIsDir(true);
        entry.PutIsUnsynced(true);
        entry.PutSpecifics(DefaultBookmarkSpecifics());
        // Set the time to 30 seconds in the future to reduce the chance of
        // flaky tests.
        const base::Time& now_plus_30s =
            base::Time::Now() + base::TimeDelta::FromSeconds(30);
        const base::Time& now_minus_2h =
            base::Time::Now() - base::TimeDelta::FromHours(2);
        entry.PutMtime(now_plus_30s);
        for (size_t i = 0 ; i < arraysize(test->features) ; ++i) {
          switch (test->features[i]) {
            case LIST_END:
              break;
            case SYNCED:
              entry.PutIsUnsynced(false);
              break;
            case DELETED:
              entry.PutIsDel(true);
              break;
            case OLD_MTIME:
              entry.PutMtime(now_minus_2h);
              break;
            case MOVED_FROM_ROOT:
              entry.PutServerParentId(trans.root_id());
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
      SCOPED_TRACE(i);
      EXPECT_EQ(1u, expected_positions.count(i));
      EXPECT_EQ(expected_positions[i], mock_server_->committed_ids()[i]);
    }
  }

  CommitCounters GetCommitCounters(ModelType type) {
    return debug_info_cache_.GetLatestCommitCounters(type);
  }

  UpdateCounters GetUpdateCounters(ModelType type) {
    return debug_info_cache_.GetLatestUpdateCounters(type);
  }

  StatusCounters GetStatusCounters(ModelType type) {
    return debug_info_cache_.GetLatestStatusCounters(type);
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
    entry.PutIsUnsynced(true);
    entry.PutIsDir(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutBaseVersion(id.ServerKnows() ? 1 : 0);
    entry.PutId(id);
    return entry.GetMetahandle();
  }

  void EnableDatatype(ModelType model_type) {
    enabled_datatypes_.Put(model_type);

    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    if (context_) {
      context_->SetRoutingInfo(routing_info);
    }

    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  void DisableDatatype(ModelType model_type) {
    enabled_datatypes_.Remove(model_type);

    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    if (context_) {
      context_->SetRoutingInfo(routing_info);
    }

    mock_server_->ExpectGetUpdatesRequestTypes(enabled_datatypes_);
  }

  Cryptographer* GetCryptographer(syncable::BaseTransaction* trans) {
    return directory()->GetCryptographer(trans);
  }

  // Configures SyncSessionContext and NudgeTracker so Syncer won't call
  // GetUpdates prior to Commit. This method can be used to ensure a Commit is
  // not preceeded by GetUpdates.
  void ConfigureNoGetUpdatesRequired() {
    context_->set_server_enabled_pre_commit_update_avoidance(true);
    nudge_tracker_.OnInvalidationsEnabled();
    nudge_tracker_.RecordSuccessfulSyncCycle();

    ASSERT_FALSE(context_->ShouldFetchUpdatesBeforeCommit());
    ASSERT_FALSE(nudge_tracker_.IsGetUpdatesRequired());
  }

  base::MessageLoop message_loop_;

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
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  scoped_ptr<MockConnectionManager> mock_server_;
  CancelationSignal cancelation_signal_;

  Syncer* syncer_;

  scoped_ptr<SyncSession> session_;
  TypeDebugInfoCache debug_info_cache_;
  MockNudgeHandler mock_nudge_handler_;
  scoped_ptr<ModelTypeRegistry> model_type_registry_;
  scoped_ptr<SyncSessionContext> context_;
  bool saw_syncer_event_;
  base::TimeDelta last_short_poll_interval_received_;
  base::TimeDelta last_long_poll_interval_received_;
  base::TimeDelta last_sessions_commit_delay_seconds_;
  int last_client_invalidation_hint_buffer_size_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;

  ModelTypeSet enabled_datatypes_;
  sessions::NudgeTracker nudge_tracker_;
  scoped_ptr<MockDebugInfoGetter> debug_info_getter_;

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
    A.PutIsUnsynced(true);
    A.PutSpecifics(bookmark_data);
    A.PutNonUniqueName("bookmark");
  }

  // Now sync without enabling bookmarks.
  mock_server_->ExpectGetUpdatesRequestTypes(
      Difference(context_->GetEnabledTypes(), ModelTypeSet(BOOKMARKS)));
  ResetSession();
  syncer_->NormalSyncShare(
      Difference(context_->GetEnabledTypes(), ModelTypeSet(BOOKMARKS)),
      nudge_tracker_,
      session_.get());

  {
    // Nothing should have been committed as bookmarks is throttled.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_TRUE(entryA.GetIsUnsynced());
  }

  // Sync again with bookmarks enabled.
  mock_server_->ExpectGetUpdatesRequestTypes(context_->GetEnabledTypes());
  SyncShareNudge();
  {
    // It should have been committed.
    syncable::ReadTransaction rtrans(FROM_HERE, directory());
    Entry entryA(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entryA.good());
    EXPECT_FALSE(entryA.GetIsUnsynced());
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
    EXPECT_TRUE(is_unsynced == entryA.GetIsUnsynced()); \
    EXPECT_TRUE(is_unapplied == entryA.GetIsUnappliedUpdate()); \
    EXPECT_TRUE(prev_initialized == \
              IsRealDataType(GetModelTypeFromSpecifics( \
                  entryA.GetBaseServerSpecifics()))); \
    EXPECT_TRUE(parent_id == -1 || \
                entryA.GetParentId()== id_fac.FromNumber(parent_id)); \
    EXPECT_EQ(version, entryA.GetBaseVersion()); \
    EXPECT_EQ(server_version, entryA.GetServerVersion()); \
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
    A.PutIsUnsynced(true);
    A.PutSpecifics(encrypted_bookmark);
    A.PutNonUniqueName(kEncryptedString);
    // Not in conflict and properly encrypted.
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.PutIsUnsynced(true);
    B.PutSpecifics(encrypted_bookmark);
    B.PutNonUniqueName(kEncryptedString);
    // Unencrypted specifics.
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(C.good());
    C.PutIsUnsynced(true);
    C.PutNonUniqueName(kEncryptedString);
    // Unencrypted non_unique_name.
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.PutIsUnsynced(true);
    D.PutSpecifics(encrypted_bookmark);
    D.PutNonUniqueName("not encrypted");
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
    C.PutSpecifics(encrypted_bookmark);
    C.PutNonUniqueName(kEncryptedString);
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.PutSpecifics(encrypted_bookmark);
    D.PutNonUniqueName(kEncryptedString);
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

  // We need to remember the exact position of our local items, so we can
  // make updates that do not modify those positions.
  UniquePosition pos1;
  UniquePosition pos2;
  UniquePosition pos3;

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

    Entry entry1(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry1.GetUniquePosition().Equals(
        entry1.GetServerUniquePosition()));
    pos1 = entry1.GetUniquePosition();
    Entry entry2(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(2));
    pos2 = entry2.GetUniquePosition();
    Entry entry3(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(3));
    pos3 = entry3.GetUniquePosition();
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
    A.PutSpecifics(modified_bookmark);
    A.PutNonUniqueName(kEncryptedString);
    A.PutIsUnsynced(true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.PutSpecifics(modified_bookmark);
    B.PutNonUniqueName(kEncryptedString);
    B.PutIsUnsynced(true);
    MutableEntry C(&wtrans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(C.good());
    C.PutSpecifics(modified_bookmark);
    C.PutNonUniqueName(kEncryptedString);
    C.PutIsUnsynced(true);
    MutableEntry D(&wtrans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(D.good());
    D.PutSpecifics(modified_pref);
    D.PutNonUniqueName(kEncryptedString);
    D.PutIsUnsynced(true);
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
  EXPECT_EQ(1, GetUpdateCounters(BOOKMARKS).num_server_overwrites);
  EXPECT_EQ(1, GetUpdateCounters(PREFERENCES).num_server_overwrites);
  EXPECT_EQ(1, GetUpdateCounters(BOOKMARKS).num_local_overwrites);

  // We successfully commited item(s).
  EXPECT_EQ(2, GetCommitCounters(BOOKMARKS).num_commits_attempted);
  EXPECT_EQ(2, GetCommitCounters(BOOKMARKS).num_commits_success);
  EXPECT_EQ(1, GetCommitCounters(PREFERENCES).num_commits_attempted);
  EXPECT_EQ(1, GetCommitCounters(PREFERENCES).num_commits_success);

  SyncShareNudge();

  // Everything should be resolved now. The local changes should have
  // overwritten the server changes for 2 and 4, while the server changes
  // overwrote the local for entry 3.
  //
  // Expect there will be no new overwrites.
  EXPECT_EQ(1, GetUpdateCounters(BOOKMARKS).num_server_overwrites);
  EXPECT_EQ(1, GetUpdateCounters(BOOKMARKS).num_local_overwrites);

  EXPECT_EQ(2, GetCommitCounters(BOOKMARKS).num_commits_success);
  EXPECT_EQ(1, GetCommitCounters(PREFERENCES).num_commits_success);

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
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutBaseVersion(1);
    parent.PutId(parent_id_);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.PutId(child_id_);
    child.PutBaseVersion(1);
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
    directory()->SetDownloadProgress(BOOKMARKS,
                                     syncable::BuildProgress(BOOKMARKS));
    directory()->SetDownloadProgress(PREFERENCES,
                                     syncable::BuildProgress(PREFERENCES));
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutBaseVersion(1);
    parent.PutId(parent_id_);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.PutId(child_id_);
    child.PutBaseVersion(1);
    WriteTestDataToEntry(&wtrans, &child);

    MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Tim");
    ASSERT_TRUE(parent2.good());
    parent2.PutIsUnsynced(true);
    parent2.PutIsDir(true);
    parent2.PutSpecifics(DefaultPreferencesSpecifics());
    parent2.PutBaseVersion(1);
    parent2.PutId(pref_node_id);
  }

  directory()->PurgeEntriesWithTypeIn(ModelTypeSet(PREFERENCES),
                                      ModelTypeSet(),
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
    directory()->SetDownloadProgress(BOOKMARKS,
                                     syncable::BuildProgress(BOOKMARKS));
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnappliedUpdate(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutBaseVersion(1);
    parent.PutId(parent_id_);
  }

  directory()->PurgeEntriesWithTypeIn(ModelTypeSet(BOOKMARKS),
                                      ModelTypeSet(),
                                      ModelTypeSet());

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
    directory()->SetDownloadProgress(BOOKMARKS,
                                     syncable::BuildProgress(BOOKMARKS));
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, syncable::CREATE, BOOKMARKS, wtrans.root_id(),
                        "Pete");
    ASSERT_TRUE(parent.good());
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutBaseVersion(1);
    parent.PutId(parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, BOOKMARKS, parent_id_,
                       "Pete");
    ASSERT_TRUE(child.good());
    child.PutId(child_id_);
    child.PutBaseVersion(1);
    WriteTestDataToEntry(&wtrans, &child);

    MutableEntry parent2(&wtrans, syncable::CREATE, PREFERENCES,
                         wtrans.root_id(), "Tim");
    ASSERT_TRUE(parent2.good());
    parent2.PutIsDir(true);
    parent2.PutSpecifics(DefaultPreferencesSpecifics());
    parent2.PutBaseVersion(1);
    parent2.PutId(TestIdFactory::MakeServer("Tim"));
  }

  directory()->PurgeEntriesWithTypeIn(ModelTypeSet(PREFERENCES, BOOKMARKS),
                                      ModelTypeSet(BOOKMARKS),
                                      ModelTypeSet());
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

TEST_F(SyncerTest, ResetVersions) {
  // Download the top level pref node and some pref items.
  mock_server_->AddUpdateDirectory(
      parent_id_, root_id_, "prefs", 1, 10, std::string(), std::string());
  mock_server_->SetLastUpdateServerTag(ModelTypeToRootTag(PREFERENCES));
  mock_server_->AddUpdatePref("id1", parent_id_.GetServerId(), "tag1", 20, 20);
  mock_server_->AddUpdatePref("id2", parent_id_.GetServerId(), "tag2", 30, 30);
  mock_server_->AddUpdatePref("id3", parent_id_.GetServerId(), "tag3", 40, 40);
  SyncShareNudge();

  {
    // Modify one of the preferences locally, mark another one as unapplied,
    // and create another unsynced preference.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&wtrans, GET_BY_CLIENT_TAG, "tag1");
    entry.PutIsUnsynced(true);

    MutableEntry entry2(&wtrans, GET_BY_CLIENT_TAG, "tag2");
    entry2.PutIsUnappliedUpdate(true);

    MutableEntry entry4(&wtrans, CREATE, PREFERENCES, parent_id_, "name");
    entry4.PutUniqueClientTag("tag4");
    entry4.PutIsUnsynced(true);
  }

  {
    // Reset the versions.
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    ASSERT_TRUE(directory()->ResetVersionsForType(&wtrans, PREFERENCES));
  }

  {
    // Verify the synced items are all with version 1 now, with
    // unsynced/unapplied state preserved.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, "tag1");
    EXPECT_EQ(1, entry.GetBaseVersion());
    EXPECT_EQ(1, entry.GetServerVersion());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    Entry entry2(&trans, GET_BY_CLIENT_TAG, "tag2");
    EXPECT_EQ(1, entry2.GetBaseVersion());
    EXPECT_EQ(1, entry2.GetServerVersion());
    EXPECT_FALSE(entry2.GetIsUnsynced());
    EXPECT_TRUE(entry2.GetIsUnappliedUpdate());
    Entry entry3(&trans, GET_BY_CLIENT_TAG, "tag3");
    EXPECT_EQ(1, entry3.GetBaseVersion());
    EXPECT_EQ(1, entry3.GetServerVersion());
    EXPECT_FALSE(entry3.GetIsUnsynced());
    EXPECT_FALSE(entry3.GetIsUnappliedUpdate());

    // Entry 4 (the locally created one) should remain the same.
    Entry entry4(&trans, GET_BY_CLIENT_TAG, "tag4");
    EXPECT_EQ(-1, entry4.GetBaseVersion());
    EXPECT_EQ(0, entry4.GetServerVersion());
    EXPECT_TRUE(entry4.GetIsUnsynced());
    EXPECT_FALSE(entry4.GetIsUnappliedUpdate());
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

TEST_F(SyncerTest, TestCommitListOrderingFourItemsTall) {
  CommitOrderingTest items[] = {
    {3, ids_.FromNumber(-2003), ids_.FromNumber(-2002)},
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
      parent.PutIsUnsynced(true);
      parent.PutIsDir(true);
      parent.PutSpecifics(DefaultBookmarkSpecifics());
      parent.PutId(ids_.FromNumber(100));
      parent.PutBaseVersion(1);
      MutableEntry child(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(100), "Bob");
      ASSERT_TRUE(child.good());
      child.PutIsUnsynced(true);
      child.PutIsDir(true);
      child.PutSpecifics(DefaultBookmarkSpecifics());
      child.PutId(ids_.FromNumber(101));
      child.PutBaseVersion(1);
      MutableEntry grandchild(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(101), "Bob");
      ASSERT_TRUE(grandchild.good());
      grandchild.PutId(ids_.FromNumber(102));
      grandchild.PutIsUnsynced(true);
      grandchild.PutSpecifics(DefaultBookmarkSpecifics());
      grandchild.PutBaseVersion(1);
    }
    {
      // Create three deleted items which deletions we expect to be sent to the
      // server.
      MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "Pete");
      ASSERT_TRUE(parent.good());
      parent.PutId(ids_.FromNumber(103));
      parent.PutIsUnsynced(true);
      parent.PutIsDir(true);
      parent.PutSpecifics(DefaultBookmarkSpecifics());
      parent.PutIsDel(true);
      parent.PutBaseVersion(1);
      parent.PutMtime(now_minus_2h);
      MutableEntry child(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(103), "Pete");
      ASSERT_TRUE(child.good());
      child.PutId(ids_.FromNumber(104));
      child.PutIsUnsynced(true);
      child.PutIsDir(true);
      child.PutSpecifics(DefaultBookmarkSpecifics());
      child.PutIsDel(true);
      child.PutBaseVersion(1);
      child.PutMtime(now_minus_2h);
      MutableEntry grandchild(
          &wtrans, CREATE, BOOKMARKS, ids_.FromNumber(104), "Pete");
      ASSERT_TRUE(grandchild.good());
      grandchild.PutId(ids_.FromNumber(105));
      grandchild.PutIsUnsynced(true);
      grandchild.PutIsDel(true);
      grandchild.PutIsDir(false);
      grandchild.PutSpecifics(DefaultBookmarkSpecifics());
      grandchild.PutBaseVersion(1);
      grandchild.PutMtime(now_minus_2h);
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
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(parent1_id);
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "2");
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    child.PutId(parent2_id);
    parent.PutBaseVersion(1);
    child.PutBaseVersion(1);
  }
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, parent1_id, "A");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(ids_.FromNumber(102));
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent1_id, "B");
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    child.PutId(ids_.FromNumber(-103));
    parent.PutBaseVersion(1);
  }
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, parent2_id, "A");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(ids_.FromNumber(-104));
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent2_id, "B");
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    child.PutId(ids_.FromNumber(105));
    child.PutBaseVersion(1);
  }

  SyncShareNudge();
  ASSERT_EQ(6u, mock_server_->committed_ids().size());

  // This strange iteration and std::count() usage is to allow the order to
  // vary.  All we really care about is that parent1_id and parent2_id are the
  // first two IDs, and that the children make up the next four.  Other than
  // that, ordering doesn't matter.

  vector<syncable::Id>::const_iterator i =
      mock_server_->committed_ids().begin();
  vector<syncable::Id>::const_iterator parents_begin = i;
  i++;
  i++;
  vector<syncable::Id>::const_iterator parents_end = i;
  vector<syncable::Id>::const_iterator children_begin = i;
  vector<syncable::Id>::const_iterator children_end =
      mock_server_->committed_ids().end();

  EXPECT_EQ(1, count(parents_begin, parents_end, parent1_id));
  EXPECT_EQ(1, count(parents_begin, parents_end, parent2_id));

  EXPECT_EQ(1, count(children_begin, children_end, ids_.FromNumber(-103)));
  EXPECT_EQ(1, count(children_begin, children_end, ids_.FromNumber(102)));
  EXPECT_EQ(1, count(children_begin, children_end, ids_.FromNumber(105)));
  EXPECT_EQ(1, count(children_begin, children_end, ids_.FromNumber(-104)));
}

TEST_F(SyncerTest, TestCommitListOrderingCounterexample) {
  syncable::Id child2_id = ids_.NewServerId();

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent(&wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "P");
    ASSERT_TRUE(parent.good());
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(parent_id_);
    MutableEntry child1(&wtrans, CREATE, BOOKMARKS, parent_id_, "1");
    ASSERT_TRUE(child1.good());
    child1.PutIsUnsynced(true);
    child1.PutId(child_id_);
    child1.PutSpecifics(DefaultBookmarkSpecifics());
    MutableEntry child2(&wtrans, CREATE, BOOKMARKS, parent_id_, "2");
    ASSERT_TRUE(child2.good());
    child2.PutIsUnsynced(true);
    child2.PutSpecifics(DefaultBookmarkSpecifics());
    child2.PutId(child2_id);

    parent.PutBaseVersion(1);
    child1.PutBaseVersion(1);
    child2.PutBaseVersion(1);
  }

  SyncShareNudge();
  ASSERT_EQ(3u, mock_server_->committed_ids().size());
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  // There are two possible valid orderings.
  if (child2_id == mock_server_->committed_ids()[1]) {
    EXPECT_TRUE(child2_id == mock_server_->committed_ids()[1]);
    EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[2]);
  } else {
    EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
    EXPECT_TRUE(child2_id == mock_server_->committed_ids()[2]);
  }
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
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(parent_id_);
    parent.PutBaseVersion(1);
  }

  syncable::Id parent2_id = ids_.NewLocalId();
  syncable::Id child_id = ids_.NewServerId();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent2(
        &wtrans, CREATE, BOOKMARKS, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.PutIsUnsynced(true);
    parent2.PutIsDir(true);
    parent2.PutSpecifics(DefaultBookmarkSpecifics());
    parent2.PutId(parent2_id);

    MutableEntry child(
        &wtrans, CREATE, BOOKMARKS, parent2_id, child_name);
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    child.PutId(child_id);
    child.PutBaseVersion(1);
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
    EXPECT_EQ(entry_1.GetNonUniqueName(), parent1_name);
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
    EXPECT_EQ(parent2_committed_id, child.GetParentId());
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
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    parent.PutId(parent_id_);
    parent.PutBaseVersion(1);
  }

  int64 meta_handle_b;
  const Id parent2_local_id = ids_.NewLocalId();
  const Id child_local_id = ids_.NewLocalId();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, directory());
    MutableEntry parent2(&wtrans, CREATE, BOOKMARKS, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.PutIsUnsynced(true);
    parent2.PutIsDir(true);
    parent2.PutSpecifics(DefaultBookmarkSpecifics());

    parent2.PutId(parent2_local_id);
    MutableEntry child(
        &wtrans, CREATE, BOOKMARKS, parent2_local_id, child_name);
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    child.PutId(child_local_id);
    meta_handle_b = child.GetMetahandle();
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
    EXPECT_TRUE(parent.GetId().ServerKnows());

    Entry parent2(&rtrans, syncable::GET_BY_ID,
                  GetOnlyEntryWithName(&rtrans, parent.GetId(), parent2_name));
    ASSERT_TRUE(parent2.good());
    EXPECT_TRUE(parent2.GetId().ServerKnows());

    // Id changed on commit, so this should fail.
    Entry local_parent2_id_entry(&rtrans,
                                 syncable::GET_BY_ID,
                                 parent2_local_id);
    ASSERT_FALSE(local_parent2_id_entry.good());

    Entry entry_b(&rtrans, syncable::GET_BY_HANDLE, meta_handle_b);
    EXPECT_TRUE(entry_b.GetId().ServerKnows());
    EXPECT_TRUE(parent2.GetId()== entry_b.GetParentId());
  }
}

TEST_F(SyncerTest, UpdateWithZeroLengthName) {
  // One illegal update
  mock_server_->AddUpdateDirectory(
      1, 0, std::string(), 1, 10, foreign_cache_guid(), "-1");
  // And one legal one that we're going to delete.
  mock_server_->AddUpdateDirectory(2, 0, "FOO", 1, 10,
                                   foreign_cache_guid(), "-2");
  SyncShareNudge();
  // Delete the legal one. The new update has a null name.
  mock_server_->AddUpdateDirectory(
      2, 0, std::string(), 2, 20, foreign_cache_guid(), "-2");
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
    EXPECT_TRUE(entry.GetIsDir());
    EXPECT_TRUE(entry.GetServerVersion()== version);
    EXPECT_TRUE(entry.GetBaseVersion()== version);
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_FALSE(entry.GetIsDel());
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
  EXPECT_EQ(
      1,
      GetUpdateCounters(BOOKMARKS).num_hierarchy_conflict_application_failures);

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
  EXPECT_EQ(
      3,
      GetUpdateCounters(BOOKMARKS).num_hierarchy_conflict_application_failures);

  // This time around, we knew that there were conflicts.
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  VerifyHierarchyConflictsReported(mock_server_->last_request());

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    // Even though it has the same name, it should work.
    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_FALSE(name_clash.GetIsUnappliedUpdate())
        << "Duplicate name SHOULD be OK.";

    Entry bad_parent(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.GetIsUnappliedUpdate())
        << "child of unknown parent should be in conflict";

    Entry bad_parent_child(&trans, GET_BY_ID, ids_.FromNumber(9));
    ASSERT_TRUE(bad_parent_child.good());
    EXPECT_TRUE(bad_parent_child.GetIsUnappliedUpdate())
        << "grandchild of unknown parent should be in conflict";

    Entry bad_parent_child2(&trans, GET_BY_ID, ids_.FromNumber(100));
    ASSERT_TRUE(bad_parent_child2.good());
    EXPECT_TRUE(bad_parent_child2.GetIsUnappliedUpdate())
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
    EXPECT_FALSE(still_a_dir.GetIsUnappliedUpdate());
    EXPECT_EQ(10u, still_a_dir.GetBaseVersion());
    EXPECT_EQ(10u, still_a_dir.GetServerVersion());
    EXPECT_TRUE(still_a_dir.GetIsDir());

    Entry rename(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(rename.good());
    EXPECT_EQ(root, rename.GetParentId());
    EXPECT_EQ("new_name", rename.GetNonUniqueName());
    EXPECT_FALSE(rename.GetIsUnappliedUpdate());
    EXPECT_TRUE(ids_.FromNumber(1) == rename.GetId());
    EXPECT_EQ(20u, rename.GetBaseVersion());

    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_EQ(root, name_clash.GetParentId());
    EXPECT_TRUE(ids_.FromNumber(2) == name_clash.GetId());
    EXPECT_EQ(10u, name_clash.GetBaseVersion());
    EXPECT_EQ("in_root", name_clash.GetNonUniqueName());

    Entry ignored_old_version(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(ignored_old_version.good());
    EXPECT_TRUE(
        ignored_old_version.GetNonUniqueName()== "newer_version");
    EXPECT_FALSE(ignored_old_version.GetIsUnappliedUpdate());
    EXPECT_EQ(20u, ignored_old_version.GetBaseVersion());

    Entry circular_parent_issue(&trans, GET_BY_ID, ids_.FromNumber(5));
    ASSERT_TRUE(circular_parent_issue.good());
    EXPECT_TRUE(circular_parent_issue.GetIsUnappliedUpdate())
        << "circular move should be in conflict";
    EXPECT_TRUE(circular_parent_issue.GetParentId()== root_id_);
    EXPECT_TRUE(circular_parent_issue.GetServerParentId()==
                ids_.FromNumber(6));
    EXPECT_EQ(10u, circular_parent_issue.GetBaseVersion());

    Entry circular_parent_target(&trans, GET_BY_ID, ids_.FromNumber(6));
    ASSERT_TRUE(circular_parent_target.good());
    EXPECT_FALSE(circular_parent_target.GetIsUnappliedUpdate());
    EXPECT_TRUE(circular_parent_issue.GetId()==
        circular_parent_target.GetParentId());
    EXPECT_EQ(10u, circular_parent_target.GetBaseVersion());
  }

  EXPECT_FALSE(saw_syncer_event_);
  EXPECT_EQ(
      4,
      GetUpdateCounters(BOOKMARKS).num_hierarchy_conflict_application_failures);
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
    entry.PutIsDir(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutIsUnsynced(true);
    metahandle_folder = entry.GetMetahandle();
  }

  // Verify it and pull the ID out of the folder.
  syncable::Id folder_id;
  int64 metahandle_entry;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(entry.good());
    folder_id = entry.GetId();
    ASSERT_TRUE(!folder_id.ServerKnows());
  }

  // Create an entry in the newly created folder.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, folder_id, "new_entry");
    ASSERT_TRUE(entry.good());
    metahandle_entry = entry.GetMetahandle();
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out of the entry.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(folder_id, entry.GetParentId());
    EXPECT_EQ("new_entry", entry.GetNonUniqueName());
    entry_id = entry.GetId();
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
    EXPECT_EQ("new_folder", folder.GetNonUniqueName());
    EXPECT_TRUE(new_version == folder.GetBaseVersion());
    EXPECT_TRUE(new_folder_id == folder.GetId());
    EXPECT_TRUE(folder.GetId().ServerKnows());
    EXPECT_EQ(trans.root_id(), folder.GetParentId());

    // Since it was updated, the old folder should not exist.
    Entry old_dead_folder(&trans, GET_BY_ID, folder_id);
    EXPECT_FALSE(old_dead_folder.good());

    // The child's parent should have changed.
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ("new_entry", entry.GetNonUniqueName());
    EXPECT_EQ(new_folder_id, entry.GetParentId());
    EXPECT_TRUE(!entry.GetId().ServerKnows());
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
    entry_metahandle = entry.GetMetahandle();
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.GetId();
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
    EXPECT_TRUE(new_version == entry.GetBaseVersion());
    EXPECT_TRUE(new_entry_id == entry.GetId());
    EXPECT_EQ("new_entry", entry.GetNonUniqueName());
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
    entry_metahandle = entry.GetMetahandle();
    WriteTestDataToEntry(&trans, &entry);
  }
  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.GetId();
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
    entry.PutIsDel(true);
  }

  // Just don't CHECK fail in sync, have the update split.
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Id new_entry_id = GetOnlyEntryWithName(
        &trans, trans.root_id(), "new_entry");
    Entry entry(&trans, GET_BY_ID, new_entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsDel());

    Entry old_entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(old_entry.good());
    EXPECT_TRUE(old_entry.GetIsDel());
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
    A.PutIsUnsynced(true);
    A.PutIsUnappliedUpdate(true);
    A.PutServerVersion(20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.PutIsUnappliedUpdate(true);
    B.PutServerVersion(20);
  }
  SyncShareNudge();
  saw_syncer_event_ = false;
  mock_server_->set_conflict_all_commits(false);

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.GetIsUnsynced()== false);
    EXPECT_TRUE(A.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(A.GetServerVersion()== 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.GetIsUnsynced()== false);
    EXPECT_TRUE(B.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(B.GetServerVersion()== 20);
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
    A.PutIsUnsynced(true);
    A.PutIsUnappliedUpdate(true);
    A.PutServerVersion(20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.PutIsUnappliedUpdate(true);
    B.PutServerVersion(20);
  }
  SyncShareNudge();
  saw_syncer_event_ = false;
  mock_server_->set_conflict_all_commits(false);

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.GetIsUnsynced()== false);
    EXPECT_TRUE(A.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(A.GetServerVersion()== 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.GetIsUnsynced()== false);
    EXPECT_TRUE(B.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(B.GetServerVersion()== 20);
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
        &trans, CREATE, BOOKMARKS, bob.GetId(), "bob");
    CHECK(entry2.good());
    entry2.PutIsDir(true);
    entry2.PutIsUnsynced(true);
    entry2.PutSpecifics(DefaultBookmarkSpecifics());
  }
};

TEST_F(EntryCreatedInNewFolderTest, EntryCreatedInNewFolderMidSync) {
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    ASSERT_TRUE(entry.good());
    entry.PutIsDir(true);
    entry.PutIsUnsynced(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
  }

  mock_server_->SetMidCommitCallback(
      base::Bind(&EntryCreatedInNewFolderTest::CreateFolderInBob,
                 base::Unretained(this)));
  SyncShareNudge();
  // We loop until no unsynced handles remain, so we will commit both ids.
  EXPECT_EQ(2u, mock_server_->committed_ids().size());
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry parent_entry(&trans, syncable::GET_BY_ID,
        GetOnlyEntryWithName(&trans, TestIdFactory::root(), "bob"));
    ASSERT_TRUE(parent_entry.good());

    Id child_id =
        GetOnlyEntryWithName(&trans, parent_entry.GetId(), "bob");
    Entry child(&trans, syncable::GET_BY_ID, child_id);
    ASSERT_TRUE(child.good());
    EXPECT_EQ(parent_entry.GetId(), child.GetParentId());
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
    metahandle_fred = fred_match.GetMetahandle();
    orig_id = fred_match.GetId();
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
    EXPECT_TRUE(fred_match.GetId().ServerKnows());
    fred_match_id = fred_match.GetId();
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
    parent.PutIsDir(true);
    parent.PutId(parent_id_);
    parent.PutBaseVersion(5);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent_id_, "Pete.htm");
    ASSERT_TRUE(child.good());
    local_id = child.GetId();
    child.PutId(child_id_);
    child.PutBaseVersion(10);
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "Pete2.htm", 11, 10,
                                  local_cache_guid(), local_id.GetServerId());
  mock_server_->set_conflict_all_commits(true);
  SyncShareNudge();
  syncable::Directory::Metahandles children;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    directory()->GetChildHandlesById(&trans, parent_id_, &children);
    // We expect the conflict resolver to preserve the local entry.
    Entry child(&trans, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(child.good());
    EXPECT_TRUE(child.GetIsUnsynced());
    EXPECT_FALSE(child.GetIsUnappliedUpdate());
    EXPECT_TRUE(child.GetSpecifics().has_bookmark());
    EXPECT_EQ("Pete.htm", child.GetNonUniqueName());
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
    EXPECT_FALSE(entry.GetId().ServerKnows());
    local_id = entry.GetId();
    entry.PutIsDir(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutIsUnsynced(true);
    entry.PutMtime(test_time);
    entry_metahandle = entry.GetMetahandle();
  }
  SyncShareNudge();
  syncable::Id id;
  int64 version;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    id = entry.GetId();
    EXPECT_TRUE(id.ServerKnows());
    version = entry.GetBaseVersion();
  }
  sync_pb::SyncEntity* update = mock_server_->AddUpdateFromLastCommit();
  update->set_originator_cache_guid(local_cache_guid());
  update->set_originator_client_item_id(local_id.GetServerId());
  EXPECT_EQ("Pete", update->name());
  EXPECT_EQ(id.GetServerId(), update->id_string());
  EXPECT_EQ(root_id_.GetServerId(), update->parent_id_string());
  EXPECT_EQ(version, update->version());
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, syncable::GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.GetMtime()== test_time);
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
    parent_local_id = parent.GetId();
    parent.PutIsDir(true);
    parent.PutIsUnsynced(true);
    parent.PutId(parent_id);
    parent.PutBaseVersion(1);
    parent.PutSpecifics(DefaultBookmarkSpecifics());

    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "test.htm");
    ASSERT_TRUE(child.good());
    child_local_id = child.GetId();
    child.PutId(child_id);
    child.PutBaseVersion(1);
    child.PutSpecifics(DefaultBookmarkSpecifics());
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
    Directory::Metahandles children;
    directory()->GetChildHandlesById(&trans, root_id_, &children);
    EXPECT_EQ(1u, children.size());
    directory()->GetChildHandlesById(&trans, parent_id, &children);
    EXPECT_EQ(1u, children.size());
    std::vector<int64> unapplied;
    directory()->GetUnappliedUpdateMetaHandles(&trans, all_types, &unapplied);
    EXPECT_EQ(0u, unapplied.size());
    syncable::Directory::Metahandles unsynced;
    directory()->GetUnsyncedMetaHandles(&trans, &unsynced);
    EXPECT_EQ(0u, unsynced.size());
    saw_syncer_event_ = false;
  }
}

TEST_F(SyncerTest, CommittingNewDeleted) {
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, CREATE, BOOKMARKS, trans.root_id(), "bob");
    entry.PutIsUnsynced(true);
    entry.PutIsDel(true);
  }
  SyncShareNudge();
  EXPECT_EQ(0u, mock_server_->committed_ids().size());
}

// Original problem synopsis:
// Check failed: entry->GetBaseVersion()<= entry->GetServerVersion()
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
    entry.PutId(ids_.FromNumber(20));
    entry.PutBaseVersion(1);
    entry.PutServerVersion(1);
    entry.PutServerParentId(ids_.FromNumber(9999));  // Bad parent.
    entry.PutIsUnsynced(true);
    entry.PutIsUnappliedUpdate(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutServerSpecifics(DefaultBookmarkSpecifics());
    entry.PutIsDel(false);
  }
  SyncShareNudge();
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
    entry.PutIsDir(true);
    entry.PutSpecifics(DefaultBookmarkSpecifics());
    entry.PutIsUnsynced(true);
    existing_metahandle = entry.GetMetahandle();
  }
  SyncShareNudge();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry newfolder(&trans, CREATE, BOOKMARKS, trans.root_id(), "new");
    ASSERT_TRUE(newfolder.good());
    newfolder.PutIsDir(true);
    newfolder.PutSpecifics(DefaultBookmarkSpecifics());
    newfolder.PutIsUnsynced(true);

    MutableEntry existing(&trans, GET_BY_HANDLE, existing_metahandle);
    ASSERT_TRUE(existing.good());
    existing.PutParentId(newfolder.GetId());
    existing.PutIsUnsynced(true);
    EXPECT_TRUE(existing.GetId().ServerKnows());

    newfolder.PutIsDel(true);
    existing.PutIsDel(true);
  }
  SyncShareNudge();
  EXPECT_EQ(0, GetCommitCounters(BOOKMARKS).num_commits_conflict);
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
    newfolder.PutIsUnsynced(true);
    newfolder.PutIsDir(true);
    newfolder.PutSpecifics(DefaultBookmarkSpecifics());
    newfolder_metahandle = newfolder.GetMetahandle();
  }
  mock_server_->AddUpdateDirectory(1, 0, "bob", 2, 20,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateDeleted();
  SyncShareConfigure();
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
    EXPECT_TRUE("fred" == id1.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id1.GetParentId());
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id2.GetParentId());
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
    EXPECT_TRUE("bob" == id1.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id1.GetParentId());
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("fred" == id2.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id2.GetParentId());
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("alice" == id3.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id3.GetParentId());
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
    EXPECT_TRUE("fred" == id1.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id1.GetParentId());
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id2.GetParentId());
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("bob" == id3.GetNonUniqueName());
    EXPECT_TRUE(root_id_ == id3.GetParentId());
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
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
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
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
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
      e.PutIsUnsynced(true);
      e.PutIsDir(true);
      e.PutSpecifics(DefaultBookmarkSpecifics());
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

// Tests that sending debug info events works.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_HappyCase) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  SyncShareNudge();

  // Verify we received one GetUpdates request with two debug info events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  SyncShareNudge();

  // See that we received another GetUpdates request, but that it contains no
  // debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());

  debug_info_getter_->AddDebugEvent();

  SyncShareNudge();

  // See that we received another GetUpdates request and it contains one debug
  // info event.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnGetUpdates_PostFailsDontDrop) {
  debug_info_getter_->AddDebugEvent();
  debug_info_getter_->AddDebugEvent();

  mock_server_->FailNextPostBufferToPathCall();
  SyncShareNudge();

  // Verify we attempted to send one GetUpdates request with two debug info
  // events.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  SyncShareNudge();

  // See that the client resent the two debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(2, mock_server_->last_request().debug_info().events_size());

  // The previous send was successful so this next one shouldn't generate any
  // debug info events.
  SyncShareNudge();
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_get_updates());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that sending debug info events on Commit works.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_HappyCase) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  CreateUnsyncedDirectory("X", "id_X");
  SyncShareNudge();

  // Verify that the last request received is a Commit and that it contains a
  // debug info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit, but no debug info event.
  CreateUnsyncedDirectory("Y", "id_Y");
  SyncShareNudge();

  // See that it was received and contains no debug info events.
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
}

// Tests that debug info events are not dropped on server error.
TEST_F(SyncerTest, SendDebugInfoEventsOnCommit_PostFailsDontDrop) {
  // Make sure GetUpdate isn't call as it would "steal" debug info events before
  // Commit has a chance to send them.
  ConfigureNoGetUpdatesRequired();

  mock_server_->FailNextPostBufferToPathCall();

  // Generate a debug info event and trigger a commit.
  debug_info_getter_->AddDebugEvent();
  CreateUnsyncedDirectory("X", "id_X");
  SyncShareNudge();

  // Verify that the last request sent is a Commit and that it contains a debug
  // info event.
  EXPECT_EQ(1U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Try again.
  SyncShareNudge();

  // Verify that we've received another Commit and that it contains a debug info
  // event (just like the previous one).
  EXPECT_EQ(2U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(1, mock_server_->last_request().debug_info().events_size());

  // Generate another commit and try again.
  CreateUnsyncedDirectory("Y", "id_Y");
  SyncShareNudge();

  // See that it was received and contains no debug info events.
  EXPECT_EQ(3U, mock_server_->requests().size());
  ASSERT_TRUE(mock_server_->last_request().has_commit());
  EXPECT_EQ(0, mock_server_->last_request().debug_info().events_size());
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
      EXPECT_TRUE(e.GetIsDel());
      EXPECT_TRUE(e.GetIsUnappliedUpdate());
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
      EXPECT_FALSE(e.GetIsDel());
      EXPECT_FALSE(e.GetIsUnappliedUpdate());
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
    e.PutIsUnsynced(true);
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
    local_folder_id = new_entry.GetId();
    local_folder_handle = new_entry.GetMetahandle();
    new_entry.PutIsUnsynced(true);
    new_entry.PutSpecifics(DefaultBookmarkSpecifics());
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
    EXPECT_TRUE(local.GetMetahandle()!= server.GetMetahandle());
    EXPECT_FALSE(server.GetIsUnappliedUpdate());
    EXPECT_FALSE(local.GetIsUnappliedUpdate());
    EXPECT_TRUE(server.GetIsUnsynced());
    EXPECT_TRUE(local.GetIsUnsynced());
    EXPECT_EQ("Foo.htm", server.GetNonUniqueName());
    EXPECT_EQ("Bar.htm", local.GetNonUniqueName());
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
    EXPECT_TRUE(local.GetMetahandle()!= server.GetMetahandle());
    EXPECT_FALSE(server.GetIsUnappliedUpdate());
    EXPECT_FALSE(local.GetIsUnappliedUpdate());
    EXPECT_FALSE(server.GetIsUnsynced());
    EXPECT_FALSE(local.GetIsUnsynced());
    EXPECT_EQ("Bar.htm", server.GetNonUniqueName());
    EXPECT_EQ("Bar.htm", local.GetNonUniqueName());
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.GetSpecifics().bookmark().url());
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
    local_folder_id = new_entry.GetId();
    local_folder_handle = new_entry.GetMetahandle();
    new_entry.PutIsUnsynced(true);
    new_entry.PutSpecifics(DefaultBookmarkSpecifics());
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
    EXPECT_TRUE(local.GetMetahandle()!= server.GetMetahandle());
    EXPECT_FALSE(server.GetIsUnappliedUpdate());
    EXPECT_FALSE(local.GetIsUnappliedUpdate());
    EXPECT_TRUE(server.GetIsUnsynced());
    EXPECT_TRUE(local.GetIsUnsynced());
    EXPECT_EQ("Foo.htm", server.GetNonUniqueName());
    EXPECT_EQ("Bar.htm", local.GetNonUniqueName());
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
    EXPECT_TRUE(local.GetMetahandle()!= server.GetMetahandle());
    EXPECT_FALSE(server.GetIsUnappliedUpdate());
    EXPECT_FALSE(local.GetIsUnappliedUpdate());
    EXPECT_FALSE(server.GetIsUnsynced());
    EXPECT_FALSE(local.GetIsUnsynced());
    EXPECT_EQ("Bar.htm", server.GetNonUniqueName());
    EXPECT_EQ("Bar.htm", local.GetNonUniqueName());
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.GetSpecifics().bookmark().url());
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
    A.PutIsUnsynced(true);
    A.PutParentId(ids_.FromNumber(2));
    A.PutNonUniqueName("B");
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
    EXPECT_TRUE(A.GetNonUniqueName()== "B");
    EXPECT_TRUE(B.GetNonUniqueName()== "B");
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
    A.PutIsUnsynced(true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.PutIsUnsynced(true);
    A.PutNonUniqueName("C");
    B.PutNonUniqueName("A");
    A.PutNonUniqueName("B");
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
    B.PutIsDel(true);
  }
  mock_server_->AddUpdateBookmark(2, 0, "A", 11, 11,
                                  foreign_cache_guid(), "-2");
  mock_server_->SetLastUpdateDeleted();
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_FALSE(B.GetIsUnsynced());
    EXPECT_FALSE(B.GetIsUnappliedUpdate());
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
    bob_metahandle = bob.GetMetahandle();
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
    EXPECT_TRUE(bob.GetIsUnsynced());
    EXPECT_TRUE(bob.GetId().ServerKnows());
    EXPECT_FALSE(bob.GetIsUnappliedUpdate());
    EXPECT_FALSE(bob.GetIsDel());
    EXPECT_EQ(2, bob.GetServerVersion());
    EXPECT_EQ(2, bob.GetBaseVersion());
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
    folder.PutIsUnsynced(true);
    folder.PutIsDir(true);
    folder.PutSpecifics(DefaultBookmarkSpecifics());
    MutableEntry folder2(&trans, CREATE, BOOKMARKS, trans.root_id(), "fred");
    ASSERT_TRUE(folder2.good());
    folder2.PutIsUnsynced(false);
    folder2.PutIsDir(true);
    folder2.PutSpecifics(DefaultBookmarkSpecifics());
    folder2.PutBaseVersion(3);
    folder2.PutId(syncable::Id::CreateFromServerId("mock_server:10000"));
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
    bob.PutParentId(ids_.FromNumber(54));
    bob.PutIsDel(true);
    bob.PutIsUnsynced(true);
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
    local_id = local_deleted.GetId();
    local_deleted.PutId(ids_.FromNumber(1));
    local_deleted.PutBaseVersion(1);
    local_deleted.PutIsDel(true);
    local_deleted.PutIsDir(false);
    local_deleted.PutIsUnsynced(true);
    local_deleted.PutSpecifics(DefaultBookmarkSpecifics());
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
    EXPECT_TRUE(local_deleted.GetBaseVersion()== 10);
    EXPECT_TRUE(local_deleted.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(local_deleted.GetIsUnsynced()== true);
    EXPECT_TRUE(local_deleted.GetIsDel()== true);
    EXPECT_TRUE(local_deleted.GetIsDir()== false);
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
    local_deleted.PutId(ids_.FromNumber(1));
    local_deleted.PutBaseVersion(1);
    local_deleted.PutIsDel(true);
    local_deleted.PutIsDir(true);
    local_deleted.PutIsUnsynced(true);
    local_deleted.PutSpecifics(DefaultBookmarkSpecifics());
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
    EXPECT_TRUE(local_deleted.GetBaseVersion()== 1);
    EXPECT_TRUE(local_deleted.GetIsUnappliedUpdate()== false);
    EXPECT_TRUE(local_deleted.GetIsUnsynced()== true);
    EXPECT_TRUE(local_deleted.GetIsDel()== true);
    EXPECT_TRUE(local_deleted.GetIsDir()== true);
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
  SyncShareNudge();
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
  SyncShareNudge();

  // Buffer up a very long series of downloads.
  // We should never be stuck (conflict resolution shouldn't
  // kick in so long as we're making forward progress).
  for (int i = 0; i < depth; i++) {
    mock_server_->NextUpdateBatch();
    mock_server_->SetNewTimestamp(i + 1);
    mock_server_->SetChangesRemaining(depth - i);
  }

  SyncShareNudge();

  // Ensure our folder hasn't somehow applied.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry child(&trans, GET_BY_ID, stuck_entry_id);
    EXPECT_TRUE(child.good());
    EXPECT_TRUE(child.GetIsUnappliedUpdate());
    EXPECT_TRUE(child.GetIsDel());
    EXPECT_FALSE(child.GetIsUnsynced());
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
    EXPECT_EQ(entry.GetId(), child.GetParentId());
    EXPECT_EQ("stuck", child.GetNonUniqueName());
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
    entry.PutNonUniqueName("Copy of base");
    entry.PutIsUnsynced(true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50,
                                  foreign_cache_guid(), "-1");
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry1(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_FALSE(entry1.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry1.GetIsUnsynced());
    EXPECT_FALSE(entry1.GetIsDel());
    Entry entry2(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_FALSE(entry2.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry2.GetIsUnsynced());
    EXPECT_FALSE(entry2.GetIsDel());
    EXPECT_EQ(entry1.GetNonUniqueName(), entry2.GetNonUniqueName());
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
    EXPECT_TRUE(entry.GetIsDel());
    metahandle = entry.GetMetahandle();
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
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_TRUE(entry.GetIsUnappliedUpdate());
    EXPECT_NE(entry.GetMetahandle(), metahandle);
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
    entry.PutParentId(ids_.FromNumber(1));
    EXPECT_TRUE(entry.PutIsUnsynced(true));
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
    EXPECT_FALSE(good_entry.GetIsUnappliedUpdate());
    Entry bad_parent(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.GetIsUnappliedUpdate());
  }
}

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
    EXPECT_EQ("in_root_name", in_root.GetNonUniqueName());
    EXPECT_EQ(TestIdFactory::root(), in_root.GetParentId());

    Entry in_in_root(&trans, GET_BY_ID, in_in_root_id);
    ASSERT_TRUE(in_in_root.good());
    EXPECT_EQ("in_in_root_name", in_in_root.GetNonUniqueName());
    EXPECT_EQ(in_root_id, in_in_root.GetParentId());
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
    parent.PutIsUnsynced(true);
    parent.PutIsDir(true);
    parent.PutSpecifics(DefaultBookmarkSpecifics());
    in_root_id = parent.GetId();
    foo_metahandle = parent.GetMetahandle();

    MutableEntry child(&wtrans, CREATE, BOOKMARKS, parent.GetId(), "bar");
    ASSERT_TRUE(child.good());
    child.PutIsUnsynced(true);
    child.PutIsDir(true);
    child.PutSpecifics(DefaultBookmarkSpecifics());
    bar_metahandle = child.GetMetahandle();
    in_dir_id = parent.GetId();
  }
  SyncShareNudge();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry fail_by_old_id_entry(&trans, GET_BY_ID, in_root_id);
    ASSERT_FALSE(fail_by_old_id_entry.good());

    Entry foo_entry(&trans, GET_BY_HANDLE, foo_metahandle);
    ASSERT_TRUE(foo_entry.good());
    EXPECT_EQ("foo", foo_entry.GetNonUniqueName());
    EXPECT_NE(foo_entry.GetId(), in_root_id);

    Entry bar_entry(&trans, GET_BY_HANDLE, bar_metahandle);
    ASSERT_TRUE(bar_entry.good());
    EXPECT_EQ("bar", bar_entry.GetNonUniqueName());
    EXPECT_NE(bar_entry.GetId(), in_dir_id);
    EXPECT_EQ(foo_entry.GetId(), bar_entry.GetParentId());
  }
}

TEST_F(SyncerTest, TestClientCommandDuringUpdate) {
  using sync_pb::ClientCommand;

  ClientCommand* command = new ClientCommand();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  command->set_client_invalidation_hint_buffer_size(11);
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
  EXPECT_EQ(11, last_client_invalidation_hint_buffer_size_);

  command = new ClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  command->set_client_invalidation_hint_buffer_size(9);
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
  EXPECT_EQ(9, last_client_invalidation_hint_buffer_size_);
}

TEST_F(SyncerTest, TestClientCommandDuringCommit) {
  using sync_pb::ClientCommand;

  ClientCommand* command = new ClientCommand();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  command->set_sessions_commit_delay_seconds(3141);
  command->set_client_invalidation_hint_buffer_size(11);
  CreateUnsyncedDirectory("X", "id_X");
  mock_server_->SetCommitClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(8) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(800) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(3141) ==
              last_sessions_commit_delay_seconds_);
  EXPECT_EQ(11, last_client_invalidation_hint_buffer_size_);

  command = new ClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  command->set_sessions_commit_delay_seconds(2718);
  command->set_client_invalidation_hint_buffer_size(9);
  CreateUnsyncedDirectory("Y", "id_Y");
  mock_server_->SetCommitClientCommand(command);
  SyncShareNudge();

  EXPECT_TRUE(TimeDelta::FromSeconds(180) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(190) ==
              last_long_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(2718) ==
              last_sessions_commit_delay_seconds_);
  EXPECT_EQ(9, last_client_invalidation_hint_buffer_size_);
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
    entry.PutParentId(folder_two_id);
    entry.PutIsUnsynced(true);
    // A new entry should send no "old parent."
    MutableEntry create(
        &trans, CREATE, BOOKMARKS, trans.root_id(), "new_folder");
    create.PutIsUnsynced(true);
    create.PutSpecifics(DefaultBookmarkSpecifics());
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
    entry.PutBaseVersion(really_big_int);
    entry.PutServerVersion(really_big_int);
    entry.PutId(ids_.NewServerId());
    item_metahandle = entry.GetMetahandle();
  }
  // Now read it back out and make sure the value is max int64.
  syncable::ReadTransaction rtrans(FROM_HERE, directory());
  Entry entry(&rtrans, syncable::GET_BY_HANDLE, item_metahandle);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(really_big_int == entry.GetBaseVersion());
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
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    // Delete it locally.
    entry.PutIsDel(true);
  }
  SyncShareNudge();
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
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
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
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
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
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
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    // Delete it locally.
    entry.PutIsDel(true);
  }
  SyncShareNudge();
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
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
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
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
    EXPECT_FALSE(perm_folder.GetIsDel());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ(perm_folder.GetUniqueClientTag(), "permfolder");
    EXPECT_EQ(perm_folder.GetNonUniqueName(), "permitem1");
  }

  mock_server_->AddUpdateDirectory(1, 0, "permitem_renamed", 10, 100,
                                   foreign_cache_guid(), "-1");
  mock_server_->SetLastUpdateClientTag("permfolder");
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry perm_folder(&trans, GET_BY_CLIENT_TAG, "permfolder");
    ASSERT_TRUE(perm_folder.good());
    EXPECT_FALSE(perm_folder.GetIsDel());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ(perm_folder.GetUniqueClientTag(), "permfolder");
    EXPECT_EQ(perm_folder.GetNonUniqueName(), "permitem_renamed");
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
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ(perm_folder.GetUniqueClientTag(), "permfolder");
    EXPECT_TRUE(perm_folder.GetNonUniqueName()== "permitem1");
    EXPECT_TRUE(perm_folder.GetId().ServerKnows());
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
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetIsUnsynced());
    EXPECT_EQ(perm_folder.GetNonUniqueName(), "permitem1");
  }
}

TEST_F(SyncerTest, ClientTagUncommittedTagMatchesUpdate) {
  int64 original_metahandle = 0;

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(
        &trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    pref.PutUniqueClientTag("tag");
    pref.PutIsUnsynced(true);
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetId().ServerKnows());
    original_metahandle = pref.GetMetahandle();
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
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_TRUE(pref.GetIsUnsynced());
    EXPECT_EQ(10, pref.GetBaseVersion());
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.GetMetahandle());
    EXPECT_EQ("tag", pref.GetUniqueClientTag());
    EXPECT_TRUE(pref.GetId().ServerKnows());
  }

  mock_server_->set_conflict_all_commits(false);
  SyncShareNudge();

  // The resolved entry ought to commit cleanly.
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    Entry pref(&trans, GET_BY_CLIENT_TAG, "tag");
    ASSERT_TRUE(pref.good());
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetIsUnsynced());
    EXPECT_TRUE(10 < pref.GetBaseVersion());
    // Entry should have been given the new ID while preserving the
    // metahandle; client should have won the conflict resolution.
    EXPECT_EQ(original_metahandle, pref.GetMetahandle());
    EXPECT_EQ("tag", pref.GetUniqueClientTag());
    EXPECT_TRUE(pref.GetId().ServerKnows());
  }
}

TEST_F(SyncerTest, ClientTagConflictWithDeletedLocalEntry) {
  {
    // Create a deleted local entry with a unique client tag.
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry pref(
        &trans, CREATE, PREFERENCES, ids_.root(), "name");
    ASSERT_TRUE(pref.good());
    ASSERT_FALSE(pref.GetId().ServerKnows());
    pref.PutUniqueClientTag("tag");
    pref.PutIsUnsynced(true);

    // Note: IS_DEL && !ServerKnows() will clear the UNSYNCED bit.
    // (We never attempt to commit server-unknown deleted items, so this
    // helps us clean up those entries).
    pref.PutIsDel(true);
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
    ASSERT_TRUE(pref.GetId().ServerKnows());
    EXPECT_FALSE(pref.GetIsDel());
    EXPECT_FALSE(pref.GetIsUnappliedUpdate());
    EXPECT_FALSE(pref.GetIsUnsynced());
    EXPECT_EQ(pref.GetBaseVersion(), 10);
    EXPECT_EQ(pref.GetUniqueClientTag(), "tag");
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
    ASSERT_TRUE(tag1.GetId().ServerKnows());
    ASSERT_TRUE(id1 == tag1.GetId());
    EXPECT_FALSE(tag1.GetIsDel());
    EXPECT_FALSE(tag1.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag1.GetIsUnsynced());
    EXPECT_EQ(10, tag1.GetBaseVersion());
    EXPECT_EQ("tag1", tag1.GetUniqueClientTag());
    tag1_metahandle = tag1.GetMetahandle();

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.GetId().ServerKnows());
    ASSERT_TRUE(id4 == tag2.GetId());
    EXPECT_FALSE(tag2.GetIsDel());
    EXPECT_FALSE(tag2.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag2.GetIsUnsynced());
    EXPECT_EQ(11, tag2.GetBaseVersion());
    EXPECT_EQ("tag2", tag2.GetUniqueClientTag());
    tag2_metahandle = tag2.GetMetahandle();

    syncable::Directory::Metahandles children;
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
    ASSERT_TRUE(tag1.GetId().ServerKnows());
    ASSERT_EQ(id1, tag1.GetId())
        << "ID 1 should be kept, since it was less than ID 2.";
    EXPECT_FALSE(tag1.GetIsDel());
    EXPECT_FALSE(tag1.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag1.GetIsUnsynced());
    EXPECT_EQ(10, tag1.GetBaseVersion());
    EXPECT_EQ("tag1", tag1.GetUniqueClientTag());
    EXPECT_EQ(tag1_metahandle, tag1.GetMetahandle());

    Entry tag2(&trans, GET_BY_CLIENT_TAG, "tag2");
    ASSERT_TRUE(tag2.good());
    ASSERT_TRUE(tag2.GetId().ServerKnows());
    ASSERT_EQ(id3, tag2.GetId())
        << "ID 3 should be kept, since it was less than ID 4.";
    EXPECT_FALSE(tag2.GetIsDel());
    EXPECT_FALSE(tag2.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag2.GetIsUnsynced());
    EXPECT_EQ(13, tag2.GetBaseVersion());
    EXPECT_EQ("tag2", tag2.GetUniqueClientTag());
    EXPECT_EQ(tag2_metahandle, tag2.GetMetahandle());

    syncable::Directory::Metahandles children;
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
    EXPECT_TRUE(tag_a.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(1), tag_a.GetId());
    EXPECT_FALSE(tag_a.GetIsDel());
    EXPECT_FALSE(tag_a.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_a.GetIsUnsynced());
    EXPECT_EQ(1, tag_a.GetBaseVersion());
    EXPECT_EQ("tag a", tag_a.GetUniqueClientTag());

    Entry tag_b(&trans, GET_BY_CLIENT_TAG, "tag b");
    ASSERT_TRUE(tag_b.good());
    EXPECT_TRUE(tag_b.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(101), tag_b.GetId());
    EXPECT_FALSE(tag_b.GetIsDel());
    EXPECT_FALSE(tag_b.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_b.GetIsUnsynced());
    EXPECT_EQ(16, tag_b.GetBaseVersion());
    EXPECT_EQ("tag b", tag_b.GetUniqueClientTag());

    Entry tag_c(&trans, GET_BY_CLIENT_TAG, "tag c");
    ASSERT_TRUE(tag_c.good());
    EXPECT_TRUE(tag_c.GetId().ServerKnows());
    EXPECT_EQ(ids_.FromNumber(201), tag_c.GetId());
    EXPECT_FALSE(tag_c.GetIsDel());
    EXPECT_FALSE(tag_c.GetIsUnappliedUpdate());
    EXPECT_FALSE(tag_c.GetIsUnsynced());
    EXPECT_EQ(21, tag_c.GetBaseVersion());
    EXPECT_EQ("tag c", tag_c.GetUniqueClientTag());

    syncable::Directory::Metahandles children;
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
    ASSERT_TRUE(!hurdle.GetIsDel());
    ASSERT_TRUE(hurdle.GetUniqueServerTag().empty());
    ASSERT_TRUE(hurdle.GetNonUniqueName()== "bob");

    // Try to lookup by the tagname.  These should fail.
    Entry tag_alpha(&trans, GET_BY_SERVER_TAG, "alpha");
    EXPECT_FALSE(tag_alpha.good());
    Entry tag_bob(&trans, GET_BY_SERVER_TAG, "bob");
    EXPECT_FALSE(tag_bob.good());
  }

  // Now download some tagged items as updates.
  mock_server_->AddUpdateDirectory(
      1, 0, "update1", 1, 10, std::string(), std::string());
  mock_server_->SetLastUpdateServerTag("alpha");
  mock_server_->AddUpdateDirectory(
      2, 0, "update2", 2, 20, std::string(), std::string());
  mock_server_->SetLastUpdateServerTag("bob");
  SyncShareNudge();

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    // The new items should be applied as new entries, and we should be able
    // to look them up by their tag values.
    Entry tag_alpha(&trans, GET_BY_SERVER_TAG, "alpha");
    ASSERT_TRUE(tag_alpha.good());
    ASSERT_TRUE(!tag_alpha.GetIsDel());
    ASSERT_TRUE(tag_alpha.GetUniqueServerTag()== "alpha");
    ASSERT_TRUE(tag_alpha.GetNonUniqueName()== "update1");
    Entry tag_bob(&trans, GET_BY_SERVER_TAG, "bob");
    ASSERT_TRUE(tag_bob.good());
    ASSERT_TRUE(!tag_bob.GetIsDel());
    ASSERT_TRUE(tag_bob.GetUniqueServerTag()== "bob");
    ASSERT_TRUE(tag_bob.GetNonUniqueName()== "update2");
    // The old item should be unchanged.
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.GetIsDel());
    ASSERT_TRUE(hurdle.GetUniqueServerTag().empty());
    ASSERT_TRUE(hurdle.GetNonUniqueName()== "bob");
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
  EXPECT_FALSE(received.GetIsUnsynced());
  EXPECT_FALSE(received.GetIsUnappliedUpdate());

  Entry committed(&trans, GET_BY_HANDLE, commit_handle);
  ASSERT_TRUE(committed.good());
  EXPECT_FALSE(committed.GetIsUnsynced());
  EXPECT_FALSE(committed.GetIsUnappliedUpdate());
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
  EXPECT_TRUE(committed.GetIsUnsynced());
  EXPECT_FALSE(committed.GetIsUnappliedUpdate());

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
  EXPECT_FALSE(n1.GetIsUnappliedUpdate());

  Entry n2(&trans, GET_BY_ID, node2);
  ASSERT_TRUE(n2.good());
  EXPECT_FALSE(n2.GetIsUnappliedUpdate());
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
  EXPECT_TRUE(n1.GetIsUnappliedUpdate());

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

  mock_server_->SetKeystoreKey(std::string());
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
    perm_folder.PutUniqueClientTag(client_tag_);
    perm_folder.PutIsUnsynced(true);
    perm_folder.PutSyncing(false);
    perm_folder.PutSpecifics(DefaultBookmarkSpecifics());
    EXPECT_FALSE(perm_folder.GetIsUnappliedUpdate());
    EXPECT_FALSE(perm_folder.GetId().ServerKnows());
    metahandle_ = perm_folder.GetMetahandle();
    local_id_ = perm_folder.GetId();
  }

  void Delete() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    entry.PutIsDel(true);
    entry.PutIsUnsynced(true);
    entry.PutSyncing(false);
  }

  void Undelete() {
    WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    MutableEntry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    entry.PutIsDel(false);
    entry.PutIsUnsynced(true);
    entry.PutSyncing(false);
  }

  int64 GetMetahandleOfTag() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);
    EXPECT_TRUE(entry.good());
    if (!entry.good()) {
      return syncable::kInvalidMetaHandle;
    }
    return entry.GetMetahandle();
  }

  void ExpectUnsyncedCreation() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());  // Never been committed.
    EXPECT_GE(0, entry.GetBaseVersion());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

  void ExpectUnsyncedUndeletion() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
    EXPECT_EQ(0, entry.GetBaseVersion());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry.GetId().ServerKnows());
  }

  void ExpectUnsyncedEdit() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_LT(0, entry.GetBaseVersion());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_TRUE(entry.GetId().ServerKnows());
  }

  void ExpectUnsyncedDeletion() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_TRUE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_LT(0, entry.GetBaseVersion());
    EXPECT_LT(0, entry.GetServerVersion());
  }

  void ExpectSyncedAndCreated() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetServerIsDel());
    EXPECT_LT(0, entry.GetBaseVersion());
    EXPECT_EQ(entry.GetBaseVersion(), entry.GetServerVersion());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

  void ExpectSyncedAndDeleted() {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_CLIENT_TAG, client_tag_);

    EXPECT_EQ(metahandle_, entry.GetMetahandle());
    EXPECT_TRUE(entry.GetIsDel());
    EXPECT_TRUE(entry.GetServerIsDel());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
    EXPECT_GE(0, entry.GetBaseVersion());
    EXPECT_GE(0, entry.GetServerVersion());
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

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);

    // Server fields lag behind.
    EXPECT_FALSE(entry.GetServerIsDel());

    // We have committed the second (undelete) update.
    EXPECT_FALSE(entry.GetIsDel());
    EXPECT_FALSE(entry.GetIsUnsynced());
    EXPECT_FALSE(entry.GetIsUnappliedUpdate());
  }

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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    EXPECT_EQ(2, entry.GetBaseVersion());
  }

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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateTombstone(entry.GetId());
  }
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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateTombstone(entry.GetId());
  }
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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateBookmark(
        entry.GetId(),
        entry.GetParentId(),
        "Thadeusz", 100, 1000,
        local_cache_guid(), local_id_.GetServerId());
  }
  mock_server_->SetLastUpdateClientTag(client_tag_);
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    EXPECT_EQ("Thadeusz", entry.GetNonUniqueName());
  }
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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    update->set_originator_client_item_id(local_id_.GetServerId());
  }
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
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    mock_server_->AddUpdateBookmark(
        entry.GetId(),
        entry.GetParentId(),
        "Thadeusz", 100, 1000,
        local_cache_guid(), local_id_.GetServerId());
  }
  mock_server_->SetLastUpdateClientTag(client_tag_);
  SyncShareNudge();
  EXPECT_EQ(0, session_->status_controller().TotalNumConflictingItems());
  EXPECT_EQ(1, mock_server_->GetAndClearNumGetUpdatesRequests());
  ExpectSyncedAndCreated();
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    Entry entry(&trans, GET_BY_HANDLE, metahandle_);
    EXPECT_EQ("Thadeusz", entry.GetNonUniqueName());
  }
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
    pref.PutIsUnsynced(true);

    MutableEntry bookmark(
        &wtrans, CREATE, BOOKMARKS, wtrans.root_id(), "bookmark");
    ASSERT_TRUE(bookmark.good());
    bookmark.PutIsUnsynced(true);

    if (ShouldFailBookmarkCommit()) {
      mock_server_->SetTransientErrorId(bookmark.GetId());
    }

    if (ShouldFailAutofillCommit()) {
      mock_server_->SetTransientErrorId(pref.GetId());
    }
  }


  // Put some extenions activity records into the monitor.
  {
    ExtensionsActivity::Records records;
    records["ABC"].extension_id = "ABC";
    records["ABC"].bookmark_write_count = 2049U;
    records["xyz"].extension_id = "xyz";
    records["xyz"].bookmark_write_count = 4U;
    context_->extensions_activity()->PutRecords(records);
  }

  SyncShareNudge();

  ExtensionsActivity::Records final_monitor_records;
  context_->extensions_activity()->GetAndClearRecords(&final_monitor_records);
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
