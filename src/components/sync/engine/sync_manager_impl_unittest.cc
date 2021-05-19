// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager_impl.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/model_type_test_util.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/cycle/sync_cycle.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/http_post_provider_interface.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_scheduler.h"
#include "components/sync/js/js_event_handler.h"
#include "components/sync/js/js_test_util.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/callback_counter.h"
#include "components/sync/test/engine/fake_sync_scheduler.h"
#include "components/sync/test/engine/test_engine_components_factory.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "url/gurl.h"

using base::ExpectDictStringValue;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace syncer {

namespace {

class TestHttpPostProviderInterface : public HttpPostProviderInterface {
 public:
  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const GURL& url) override {}
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}
  bool MakeSynchronousPost(int* net_error_code,
                           int* http_status_code) override {
    return false;
  }
  int GetResponseContentLength() const override { return 0; }
  const char* GetResponseContent() const override { return ""; }
  const std::string GetResponseHeaderValue(
      const std::string& name) const override {
    return std::string();
  }
  void Abort() override {}

 private:
  ~TestHttpPostProviderInterface() override = default;
};

class TestHttpPostProviderFactory : public HttpPostProviderFactory {
 public:
  ~TestHttpPostProviderFactory() override = default;
  scoped_refptr<HttpPostProviderInterface> Create() override {
    return new TestHttpPostProviderInterface();
  }
};

class SyncManagerObserverMock : public SyncManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSyncCycleCompleted,
              (const SyncCycleSnapshot&),
              (override));
  // NOLINT
  MOCK_METHOD(void,
              OnInitializationComplete,
              (const WeakHandle<JsBackend>&,
               const WeakHandle<DataTypeDebugInfoListener>&,
               bool),
              (override));
  // NOLINT
  MOCK_METHOD(void, OnConnectionStatusChange, (ConnectionStatus), (override));
  // NOLINT
  MOCK_METHOD(void, OnActionableError, (const SyncProtocolError&), (override));
  // NOLINT
  MOCK_METHOD(void, OnMigrationRequested, (ModelTypeSet), (override));
  // NOLINT
  MOCK_METHOD(void, OnProtocolEvent, (const ProtocolEvent&), (override));
  // NOLINT
};

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD(void,
              OnPassphraseRequired,
              (const KeyDerivationParams&, const sync_pb::EncryptedData&),
              (override));
  // NOLINT
  MOCK_METHOD(void, OnPassphraseAccepted, (), (override));
  // NOLINT
  MOCK_METHOD(void, OnTrustedVaultKeyRequired, (), (override));
  // NOLINT
  MOCK_METHOD(void, OnTrustedVaultKeyAccepted, (), (override));
  // NOLINT
  MOCK_METHOD(void,
              OnBootstrapTokenUpdated,
              (const std::string&, BootstrapTokenType type),
              (override));
  // NOLINT
  MOCK_METHOD(void, OnEncryptedTypesChanged, (ModelTypeSet, bool), (override));
  // NOLINT
  MOCK_METHOD(void,
              OnCryptographerStateChanged,
              (Cryptographer*, bool),
              (override));
  // NOLINT
  MOCK_METHOD(void,
              OnPassphraseTypeChanged,
              (PassphraseType, base::Time),
              (override));
  // NOLINT
};

}  // namespace

class SyncManagerTest : public testing::Test {
 protected:
  SyncManagerTest()
      : sync_manager_("Test sync manager",
                      network::TestNetworkConnectionTracker::GetInstance()) {}

  ~SyncManagerTest() override {}

  virtual void DoSetUp(bool enable_local_sync_backend) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    extensions_activity_ = new ExtensionsActivity();

    sync_manager_.AddObserver(&manager_observer_);
    EXPECT_CALL(manager_observer_, OnInitializationComplete)
        .WillOnce(DoAll(SaveArg<0>(&js_backend_),
                        SaveArg<2>(&initialization_succeeded_)));

    EXPECT_FALSE(js_backend_.IsInitialized());

    auto encryption_observer =
        std::make_unique<StrictMock<SyncEncryptionHandlerObserverMock>>();
    encryption_observer_ = encryption_observer.get();

    SyncManager::InitArgs args;
    args.service_url = GURL("https://example.com/");
    args.post_factory = std::make_unique<TestHttpPostProviderFactory>();
    args.encryption_observer_proxy = std::move(encryption_observer);
    args.extensions_activity = extensions_activity_.get();
    args.cache_guid = "fake_cache_guid";
    args.invalidator_client_id = "fake_invalidator_client_id";
    args.enable_local_sync_backend = enable_local_sync_backend;
    args.local_sync_backend_folder = temp_dir_.GetPath();
    args.engine_components_factory.reset(GetFactory());
    args.encryption_handler = &encryption_handler_;
    args.cancelation_signal = &cancelation_signal_;
    args.poll_interval = base::TimeDelta::FromMinutes(60);
    sync_manager_.Init(&args);

    EXPECT_TRUE(js_backend_.IsInitialized());

    PumpLoop();
  }

  // Test implementation.
  void SetUp() override { DoSetUp(false); }

  void TearDown() override {
    sync_manager_.RemoveObserver(&manager_observer_);
    sync_manager_.ShutdownOnSyncThread();
    PumpLoop();
  }

  ModelTypeSet GetEnabledTypes() {
    ModelTypeSet enabled_types;
    enabled_types.Put(NIGORI);
    enabled_types.Put(DEVICE_INFO);
    enabled_types.Put(BOOKMARKS);
    enabled_types.Put(THEMES);
    enabled_types.Put(SESSIONS);
    enabled_types.Put(PASSWORDS);
    enabled_types.Put(PREFERENCES);
    enabled_types.Put(PRIORITY_PREFERENCES);

    return enabled_types;
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler) {
    js_backend_.Call(FROM_HERE, &JsBackend::SetJsEventHandler, event_handler);
    PumpLoop();
  }

  virtual EngineComponentsFactory* GetFactory() {
    return new TestEngineComponentsFactory();
  }

 private:
  // Needed by |sync_manager_|.
  base::test::TaskEnvironment task_environment_;
  // Needed by |sync_manager_|.
  base::ScopedTempDir temp_dir_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;

 protected:
  FakeSyncEncryptionHandler encryption_handler_;
  SyncManagerImpl sync_manager_;
  CancelationSignal cancelation_signal_;
  WeakHandle<JsBackend> js_backend_;
  bool initialization_succeeded_;
  StrictMock<SyncManagerObserverMock> manager_observer_;
  // Owned by |sync_manager_|.
  StrictMock<SyncEncryptionHandlerObserverMock>* encryption_observer_;
};

class SyncManagerWithLocalBackendTest : public SyncManagerTest {
 protected:
  void SetUp() override { DoSetUp(true); }
};

class MockSyncScheduler : public FakeSyncScheduler {
 public:
  MockSyncScheduler() = default;
  ~MockSyncScheduler() override = default;
  MOCK_METHOD(void, Start, (SyncScheduler::Mode, base::Time), (override));
  void ScheduleConfiguration(ConfigurationParams params) override {
    ScheduleConfiguration_(params);
  }
  MOCK_METHOD(void, ScheduleConfiguration_, (ConfigurationParams&), ());
};

class ComponentsFactory : public TestEngineComponentsFactory {
 public:
  ComponentsFactory(SyncScheduler* scheduler_to_use,
                    SyncCycleContext** cycle_context)
      : scheduler_to_use_(scheduler_to_use), cycle_context_(cycle_context) {}
  ~ComponentsFactory() override {}

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* stop_handle,
      bool local_sync_backend_enabled) override {
    *cycle_context_ = context;
    return std::move(scheduler_to_use_);
  }

 private:
  std::unique_ptr<SyncScheduler> scheduler_to_use_;
  SyncCycleContext** cycle_context_;
};

class SyncManagerTestWithMockScheduler : public SyncManagerTest {
 public:
  SyncManagerTestWithMockScheduler() : scheduler_(nullptr) {}
  EngineComponentsFactory* GetFactory() override {
    scheduler_ = new MockSyncScheduler();
    return new ComponentsFactory(scheduler_, &cycle_context_);
  }

  MockSyncScheduler* scheduler() { return scheduler_; }
  SyncCycleContext* cycle_context() { return cycle_context_; }

 private:
  MockSyncScheduler* scheduler_;
  SyncCycleContext* cycle_context_;
};

// Test that the configuration params are properly created and sent to
// ScheduleConfigure. No callback should be invoked.
TEST_F(SyncManagerTestWithMockScheduler, BasicConfiguration) {
  ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION;
  ModelTypeSet types_to_download(BOOKMARKS, PREFERENCES);

  ConfigurationParams params;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE, _));
  EXPECT_CALL(*scheduler(), ScheduleConfiguration_)
      .WillOnce(MoveArg<0>(&params));

  CallbackCounter ready_task_counter;
  sync_manager_.ConfigureSyncer(
      reason, types_to_download, SyncManager::SyncFeatureState::ON,
      base::BindOnce(&CallbackCounter::Callback,
                     base::Unretained(&ready_task_counter)));
  EXPECT_EQ(0, ready_task_counter.times_called());
  EXPECT_EQ(sync_pb::SyncEnums::RECONFIGURATION, params.origin);
  EXPECT_EQ(types_to_download, params.types_to_download);
}

}  // namespace syncer
