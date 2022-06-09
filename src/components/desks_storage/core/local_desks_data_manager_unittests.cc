// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager.h"

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

constexpr char kJunkData[] = "This is not valid template data.";
constexpr char kTemplateFileNameFormat[] = "%s.template";
constexpr char kUuidFormat[] = "1c186d5a-502e-49ce-9ee1-00000000000%d";
constexpr char kTemplateNameFormat[] = "desk_%d";
constexpr char kDeskOneTemplateDuplicateExpectedName[] = "desk_01 (1)";
constexpr char kDeskOneTemplateDuplicateTwoExpectedName[] = "desk_01 (2)";
constexpr char kDuplicatePatternMatchingNamedDeskExpectedNameOne[] =
    "(1) desk_template (1)";
constexpr char kDuplicatePatternMatchingNamedDeskExpectedNameTwo[] =
    "(1) desk_template (2)";

const base::FilePath kInvalidFilePath = base::FilePath("?");
const std::string kTestUuid1 = base::StringPrintf(kUuidFormat, 1);
const std::string kTestUuid2 = base::StringPrintf(kUuidFormat, 2);
const std::string kTestUuid3 = base::StringPrintf(kUuidFormat, 3);
const std::string kTestUuid4 = base::StringPrintf(kUuidFormat, 4);
const std::string kTestUuid5 = base::StringPrintf(kUuidFormat, 5);
const std::string kTestUuid6 = base::StringPrintf(kUuidFormat, 6);
const std::string kTestUuid7 = base::StringPrintf(kUuidFormat, 7);
const std::string kTestUuid8 = base::StringPrintf(kUuidFormat, 8);
const std::string kTestUuid9 = base::StringPrintf(kUuidFormat, 9);
const base::Time kTestTime1 = base::Time();
const std::string kTestFileName1 =
    base::StringPrintf(kTemplateFileNameFormat, kTestUuid1.c_str());
const std::string kPolicyWithOneTemplate =
    "[{\"version\":1,\"uuid\":\"" + kTestUuid9 +
    "\",\"name\":\""
    "Example Template"
    "\",\"created_time_usec\":\"1633535632\",\"updated_time_usec\": "
    "\"1633535632\",\"desk\":{\"apps\":[{\"window_"
    "bound\":{\"left\":0,\"top\":1,\"height\":121,\"width\":120},\"window_"
    "state\":\"NORMAL\",\"z_index\":1,\"app_type\":\"BROWSER\",\"tabs\":[{"
    "\"url\":\"https://example.com\",\"title\":\"Example\"},{\"url\":\"https://"
    "example.com/"
    "2\",\"title\":\"Example2\"}],\"active_tab_index\":1,\"window_id\":0,"
    "\"display_id\":\"100\",\"pre_minimized_window_state\":\"NORMAL\"}]}}]";

// Search |entry_list| for |entry_query| as a uuid and returns true if
// found, false if not.
bool FindUuidInUuidList(const std::string& uuid_query,
                        const std::vector<ash::DeskTemplate*>& entry_list) {
  base::GUID guid = base::GUID::ParseCaseInsensitive(uuid_query);
  DCHECK(guid.is_valid());

  for (auto* entry : entry_list) {
    if (entry->uuid() == guid)
      return true;
  }

  return false;
}

// Takes in a vector of DeskTemplate pointers and a uuid, returns a pointer to
// the DeskTemplate with matching uuid if found in vector, nullptr if not.
ash::DeskTemplate* FindEntryInEntryList(
    const std::string& uuid_string,
    const std::vector<ash::DeskTemplate*>& entries) {
  base::GUID uuid = base::GUID::ParseLowercase(uuid_string);
  auto found_entry = std::find_if(
      entries.begin(), entries.end(),
      [&uuid](ash::DeskTemplate* entry) { return uuid == entry->uuid(); });

  return found_entry != entries.end() ? *found_entry : nullptr;
}

// Verifies that the status passed into it is kOk
void VerifyEntryAddedCorrectly(DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kOk);
}

void VerifyEntryAddedErrorHitMaximumLimit(
    DeskModel::AddOrUpdateEntryStatus status) {
  EXPECT_EQ(status, DeskModel::AddOrUpdateEntryStatus::kHitMaximumLimit);
}

void WriteJunkData(const base::FilePath& temp_dir) {
  base::FilePath full_path = temp_dir.Append(kTestFileName1);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  EXPECT_TRUE(base::WriteFile(full_path, kJunkData));
}

// Make test desk template with ID containing the index.
std::unique_ptr<ash::DeskTemplate> MakeTestDeskTemplate(int index) {
  const std::string template_uuid = base::StringPrintf(kUuidFormat, index);
  const std::string template_name =
      base::StringPrintf(kTemplateNameFormat, index);
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(template_uuid,
                                          ash::DeskTemplateSource::kUser,
                                          template_name, base::Time::Now());
  desk_template->set_desk_restore_data(
      std::make_unique<app_restore::RestoreData>());
  return desk_template;
}

}  // namespace

class LocalDeskDataManagerTest : public testing::Test {
 public:
  LocalDeskDataManagerTest()
      : sample_desk_template_one_(
            std::make_unique<ash::DeskTemplate>(kTestUuid1,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_01"),
                                                kTestTime1)),
        sample_desk_template_one_duplicate_(
            std::make_unique<ash::DeskTemplate>(kTestUuid5,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_01"),
                                                base::Time::Now())),
        sample_desk_template_one_duplicate_two_(
            std::make_unique<ash::DeskTemplate>(kTestUuid6,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_01"),
                                                base::Time::Now())),
        duplicate_pattern_matching_named_desk_(
            std::make_unique<ash::DeskTemplate>(
                kTestUuid7,
                ash::DeskTemplateSource::kUser,
                std::string("(1) desk_template"),
                base::Time::Now())),
        duplicate_pattern_matching_named_desk_two_(
            std::make_unique<ash::DeskTemplate>(
                kTestUuid8,
                ash::DeskTemplateSource::kUser,
                std::string("(1) desk_template"),
                base::Time::Now())),
        duplicate_pattern_matching_named_desk_three_(
            std::make_unique<ash::DeskTemplate>(
                kTestUuid9,
                ash::DeskTemplateSource::kUser,
                std::string("(1) desk_template"),
                base::Time::Now())),
        sample_desk_template_two_(
            std::make_unique<ash::DeskTemplate>(kTestUuid2,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_02"),
                                                base::Time::Now())),
        sample_desk_template_three_(
            std::make_unique<ash::DeskTemplate>(kTestUuid3,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_03"),
                                                base::Time::Now())),
        modified_sample_desk_template_one_(
            std::make_unique<ash::DeskTemplate>(kTestUuid1,
                                                ash::DeskTemplateSource::kUser,
                                                std::string("desk_01_mod"),
                                                kTestTime1)),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        data_manager_(std::unique_ptr<LocalDeskDataManager>()) {
    sample_desk_template_one_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    sample_desk_template_one_duplicate_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    sample_desk_template_one_duplicate_two_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    duplicate_pattern_matching_named_desk_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    duplicate_pattern_matching_named_desk_two_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    duplicate_pattern_matching_named_desk_three_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    sample_desk_template_two_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    sample_desk_template_three_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
    modified_sample_desk_template_one_->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
  }

  LocalDeskDataManagerTest(const LocalDeskDataManagerTest&) = delete;
  LocalDeskDataManagerTest& operator=(const LocalDeskDataManagerTest&) = delete;

  ~LocalDeskDataManagerTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    data_manager_ = std::make_unique<LocalDeskDataManager>(temp_dir_.GetPath());
    testing::Test::SetUp();
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_duplicate_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_one_duplicate_two_;
  std::unique_ptr<ash::DeskTemplate> duplicate_pattern_matching_named_desk_;
  std::unique_ptr<ash::DeskTemplate> duplicate_pattern_matching_named_desk_two_;
  std::unique_ptr<ash::DeskTemplate>
      duplicate_pattern_matching_named_desk_three_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_two_;
  std::unique_ptr<ash::DeskTemplate> sample_desk_template_three_;
  std::unique_ptr<ash::DeskTemplate> modified_sample_desk_template_one_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LocalDeskDataManager> data_manager_;
};

TEST_F(LocalDeskDataManagerTest, CanAddEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, ReturnsErrorWhenAddingTooManyEntry) {
  for (std::size_t index = 0u; index < data_manager_->GetMaxEntryCount();
       ++index) {
    data_manager_->AddOrUpdateEntry(MakeTestDeskTemplate(index),
                                    base::BindOnce(&VerifyEntryAddedCorrectly));
  }

  data_manager_->AddOrUpdateEntry(
      MakeTestDeskTemplate(data_manager_->GetMaxEntryCount() + 1),
      base::BindOnce(&VerifyEntryAddedErrorHitMaximumLimit));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest, CanGetAllEntries) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 3ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid3, entries));

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(kTestUuid4, entries));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, GetAllEntriesIncludesPolicyValues) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->SetPolicyDeskTemplates(kPolicyWithOneTemplate);

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 4ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid2, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid3, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid9, entries));

        // One of these templates should be from policy.
        EXPECT_EQ(
            base::ranges::count_if(entries,
                                   [](const ash::DeskTemplate* entry) {
                                     return entry->source() ==
                                            ash::DeskTemplateSource::kPolicy;
                                   }),
            1l);

        // Sanity check for the search function.
        EXPECT_FALSE(FindUuidInUuidList(kTestUuid4, entries));
        loop.Quit();
      }));
  loop.Run();

  data_manager_->SetPolicyDeskTemplates("");
}

TEST_F(LocalDeskDataManagerTest, CanMarkDuplicateEntryNames) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(
      std::move(sample_desk_template_one_duplicate_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(
      std::move(sample_desk_template_one_duplicate_two_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 3ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid1, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid5, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid6, entries));

        ash::DeskTemplate* duplicate_one =
            FindEntryInEntryList(kTestUuid5, entries);
        EXPECT_NE(duplicate_one, nullptr);
        EXPECT_EQ(base::UTF16ToUTF8(duplicate_one->template_name()),
                  std::string(kDeskOneTemplateDuplicateExpectedName));

        ash::DeskTemplate* duplicate_two =
            FindEntryInEntryList(kTestUuid6, entries);
        EXPECT_NE(duplicate_two, nullptr);
        EXPECT_EQ(base::UTF16ToUTF8(duplicate_two->template_name()),
                  std::string(kDeskOneTemplateDuplicateTwoExpectedName));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, AppendsDuplicateMarkingsCorrectly) {
  data_manager_->AddOrUpdateEntry(
      std::move(duplicate_pattern_matching_named_desk_),
      base::BindOnce(&VerifyEntryAddedCorrectly));
  data_manager_->AddOrUpdateEntry(
      std::move(duplicate_pattern_matching_named_desk_two_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(
      std::move(duplicate_pattern_matching_named_desk_three_),
      base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 3ul);
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid7, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid8, entries));
        EXPECT_TRUE(FindUuidInUuidList(kTestUuid9, entries));

        ash::DeskTemplate* duplicate_one =
            FindEntryInEntryList(kTestUuid8, entries);
        EXPECT_NE(duplicate_one, nullptr);
        EXPECT_EQ(
            base::UTF16ToUTF8(duplicate_one->template_name()),
            std::string(kDuplicatePatternMatchingNamedDeskExpectedNameOne));

        ash::DeskTemplate* duplicate_two =
            FindEntryInEntryList(kTestUuid9, entries);
        EXPECT_NE(duplicate_two, nullptr);
        EXPECT_EQ(
            base::UTF16ToUTF8(duplicate_two->template_name()),
            std::string(kDuplicatePatternMatchingNamedDeskExpectedNameTwo));
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanGetEntryByUuid) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(entry->template_name(),
                  base::UTF8ToUTF16(std::string("desk_01")));
        EXPECT_EQ(entry->created_time(), kTestTime1);
      }));

  task_environment_.RunUntilIdle();
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsNotFoundIfEntryDoesNotExist) {
  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, DeskTemplateIsIgnoredIfItHasBadData) {
  auto task_runner = task_environment_.GetMainThreadTaskRunner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&WriteJunkData, temp_dir_.GetPath()));

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kNotFound, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest,
       GetEntryByUuidReturnsFailureIfDeskManagerHasInvalidPath) {
  data_manager_ = std::make_unique<LocalDeskDataManager>(kInvalidFilePath);

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kFailure, status);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanUpdateEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(modified_sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;
  data_manager_->GetEntryByUUID(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::GetEntryByUuidStatus status,
                                     std::unique_ptr<ash::DeskTemplate> entry) {
        EXPECT_EQ(DeskModel::GetEntryByUuidStatus::kOk, status);

        EXPECT_EQ(entry->uuid(), base::GUID::ParseCaseInsensitive(kTestUuid1));
        EXPECT_EQ(entry->template_name(),
                  base::UTF8ToUTF16(std::string("desk_01_mod")));
        EXPECT_EQ(entry->created_time(), kTestTime1);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanDeleteEntry) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;

  data_manager_->DeleteEntry(
      kTestUuid1,
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 0ul);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(LocalDeskDataManagerTest, CanDeleteAllEntries) {
  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_one_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_two_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  data_manager_->AddOrUpdateEntry(std::move(sample_desk_template_three_),
                                  base::BindOnce(&VerifyEntryAddedCorrectly));

  base::RunLoop loop;

  data_manager_->DeleteAllEntries(
      base::BindLambdaForTesting([&](DeskModel::DeleteEntryStatus status) {
        EXPECT_EQ(status, DeskModel::DeleteEntryStatus::kOk);
      }));

  data_manager_->GetAllEntries(base::BindLambdaForTesting(
      [&](DeskModel::GetAllEntriesStatus status,
          const std::vector<ash::DeskTemplate*>& entries) {
        EXPECT_EQ(status, DeskModel::GetAllEntriesStatus::kOk);
        EXPECT_EQ(entries.size(), 0ul);
        loop.Quit();
      }));
  loop.Run();
}

}  // namespace desks_storage
