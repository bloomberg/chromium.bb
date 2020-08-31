// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_logger.h"

#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/value_builder.h"

using testing::_;
using testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kStatefulMountPath[] = "/mnt/stateful_partition";
// The extension ids used here should be valid extension ids.
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.

const int kTimestamp = 123456;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptTimestamp, expected, "event matches") {
  em::ExtensionInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_timestamp();

  em::ExtensionInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_timestamp();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

ACTION_TEMPLATE(SaveTimestamp,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).timestamp();
}

int64_t GetCurrentTimestamp() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
}

class MockExtensionInstallEventLoggerDelegate
    : public ExtensionInstallEventLogger::Delegate {
 public:
  MockExtensionInstallEventLoggerDelegate() = default;

  MOCK_METHOD2(Add,
               void(const std::set<extensions::ExtensionId>& extensions,
                    const em::ExtensionInstallReportLogEvent& event));
};

}  // namespace

class ExtensionInstallEventLoggerTest : public testing::Test {
 protected:
  ExtensionInstallEventLoggerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        prefs_(profile_.GetTestingPrefService()),
        registry_(extensions::ExtensionRegistry::Get(&profile_)) {}

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::DBusThreadManager::Initialize();
    chromeos::PowerManagerClient::InitializeFake();

    chromeos::NetworkHandler::Initialize();

    disk_mount_manager_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_);
    disk_mount_manager_->CreateDiskEntryForMountDevice(
        chromeos::disks::DiskMountManager::MountPointInfo(
            "/dummy/device/usb", kStatefulMountPath,
            chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE),
        "device_id", "device_label", "vendor", "product",
        chromeos::DEVICE_TYPE_UNKNOWN, 1 << 20 /* total_size_in_bytes */,
        false /* is_parent */, false /* has_media */, true /* on_boot_device */,
        true /* on_removable_device */, "ext4");
  }

  void TearDown() override {
    logger_.reset();
    task_environment_.RunUntilIdle();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::disks::DiskMountManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void SetupForceList() {
    std::unique_ptr<base::Value> dict =
        extensions::DictionaryBuilder()
            .Set(kExtensionId1,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Set(kExtensionId2,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Build();
    prefs_->SetManagedPref(extensions::pref_names::kInstallForceList,
                           std::move(dict));
  }

  // Runs |function|, verifies that the expected event is added to the logs for
  // all apps in |extensions_| and its timestamp is set to the time at which the
  // |function| is run.
  template <typename T>
  void RunAndVerifyAdd(T function,
                       const std::set<extensions::ExtensionId>& extensions_) {
    Mock::VerifyAndClearExpectations(&delegate_);

    int64_t timestamp = 0;
    EXPECT_CALL(delegate_, Add(extensions_, MatchEventExceptTimestamp(event_)))
        .WillOnce(SaveTimestamp<1>(&timestamp));
    const int64_t before = GetCurrentTimestamp();
    function();
    const int64_t after = GetCurrentTimestamp();
    Mock::VerifyAndClearExpectations(&delegate_);

    EXPECT_LE(before, timestamp);
    EXPECT_GE(after, timestamp);
  }

  void CreateLogger() {
    logger_ = std::make_unique<ExtensionInstallEventLogger>(
        &delegate_, &profile_, registry_);
    event_.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;

  // Owned by chromeos::disks::DiskMountManager.
  chromeos::disks::MockDiskMountManager* disk_mount_manager_ = nullptr;

  sync_preferences::TestingPrefServiceSyncable* prefs_;
  extensions::ExtensionRegistry* registry_;
  MockExtensionInstallEventLoggerDelegate delegate_;

  em::ExtensionInstallReportLogEvent event_;

  std::unique_ptr<ExtensionInstallEventLogger> logger_;
};

// Adds an event with a timestamp. Verifies that the event is added to the log
// and the timestamp is not changed.
TEST_F(ExtensionInstallEventLoggerTest, Add) {
  CreateLogger();

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);

  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
                             MatchProto(event_)));
  logger_->Add(kExtensionId1, false /* gather_disk_space_info */,
               std::move(event));
}

// Adds an event without a timestamp. Verifies that the event is added to the
// log and the timestamp is set to the current time.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsTimestamp) {
  CreateLogger();

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);

  RunAndVerifyAdd(
      [&]() {
        logger_->Add(kExtensionId1, false /* gather_disk_space_info */,
                     std::move(event));
      },
      {kExtensionId1});
}

// Adds an event with a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsDiskSpaceInfo) {
  CreateLogger();

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);
  EXPECT_CALL(*disk_mount_manager_, disks()).Times(0);
  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  logger_->Add(kExtensionId1, true /* gather_disk_space_info */,
               std::move(event));
  Mock::VerifyAndClearExpectations(disk_mount_manager_);
  Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(*disk_mount_manager_, disks());
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
                             MatchProto(event_)));
  task_environment_.RunUntilIdle();
}

// Adds an event without a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added and its timestamp is set to the current time before posting
// the background task.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsTimestampAndDiskSpaceInfo) {
  CreateLogger();

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);

  EXPECT_CALL(*disk_mount_manager_, disks()).Times(0);
  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  const int64_t before = GetCurrentTimestamp();
  logger_->Add(kExtensionId1, true /* gather_disk_space_info */,
               std::move(event));
  const int64_t after = GetCurrentTimestamp();
  Mock::VerifyAndClearExpectations(disk_mount_manager_);
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t timestamp = 0;
  EXPECT_CALL(*disk_mount_manager_, disks());
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
                             MatchEventExceptTimestamp(event_)))
      .WillOnce(SaveTimestamp<1>(&timestamp));
  task_environment_.RunUntilIdle();

  EXPECT_LE(before, timestamp);
  EXPECT_GE(after, timestamp);
}

TEST_F(ExtensionInstallEventLoggerTest, UpdatePolicy) {
  CreateLogger();
  SetupForceList();

  // Expected new extensions_ added with disk info.
  event_.set_event_type(em::ExtensionInstallReportLogEvent::POLICY_REQUEST);
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1,
                                                               kExtensionId2},
                             MatchEventExceptTimestamp(event_)));
  EXPECT_CALL(*disk_mount_manager_, disks());
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);
}

}  // namespace policy
