// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_handler.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/zlib/google/zip_reader.h"

using testing::Pair;
using testing::UnorderedElementsAre;

const char kTestDataToWriteOnFile[] = "fake data to write to file for testing";

// TestDataCollector implements DataCollector functions for testing.
class TestDataCollector : public DataCollector {
 public:
  explicit TestDataCollector(std::string name, bool error)
      : DataCollector(), name_(name), error_(error) {}
  ~TestDataCollector() override = default;

  // Overrides from DataCollector.
  std::string GetName() const override { return name_; }

  std::string GetDescription() const override {
    return "The data collector that will be used for testing";
  }

  const PIIMap& GetDetectedPII() override { return pii_map_; }

  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback) override {
    // Add fake PII for testing and return error to the callback if required.
    PrepareDataCollectionOutput(std::move(on_data_collected_callback));
  }

  void ExportCollectedDataWithPII(
      std::set<PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      DataCollectorDoneCallback on_exported_callback) override {
    on_exported_callback = base::BindPostTask(
        base::ThreadTaskRunnerHandle::Get(), std::move(on_exported_callback));
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&TestDataCollector::WriteFileForTesting,
                       base::Unretained(this), target_directory,
                       std::move(on_exported_callback)));
  }

 private:
  // Adds an entry to the PIIMap with the name of the data collector. The
  // PIIType is not significant at this point so we use a random one. Then runs
  // `callback`. Adds errors when running `callback` if this.error_ is true.
  void PrepareDataCollectionOutput(DataCollectorDoneCallback callback) {
    if (error_) {
      std::move(callback).Run(SupportToolError::kTestDataCollectorError);
    } else {
      pii_map_.insert(std::pair<PIIType, std::string>(
          PIIType::kUIHierarchyWindowTitles, name_));
      std::move(callback).Run(absl::nullopt);
    }
  }

  // Writes fake test data to the given `target_directory` under a file named as
  // `TestDataCollector.name_`.
  void WriteFileForTesting(base::FilePath target_directory,
                           DataCollectorDoneCallback callback) {
    if (error_) {
      std::move(callback).Run(SupportToolError::kTestDataCollectorError);
    } else {
      base::FilePath target_file = target_directory.AppendASCII(name_);
      base::WriteFile(target_file, kTestDataToWriteOnFile);
      std::move(callback).Run(absl::nullopt);
    }
  }

  // Name of the TestDataCollector. It will be used to create fake PII inside
  // the PIIMap.
  std::string name_;
  // If error is true, the TestDataCollector will return error messages to the
  // callbacks in the CollectDataAndDetectPII and ExportCollectedDataWithPII
  // functions.
  bool error_;
  PIIMap pii_map_;
  base::WeakPtrFactory<TestDataCollector> weak_ptr_factory_{this};
};

class SupportToolHandlerTest : public ::testing::Test {
 public:
  SupportToolHandlerTest() = default;

  SupportToolHandlerTest(const SupportToolHandlerTest&) = delete;
  SupportToolHandlerTest& operator=(const SupportToolHandlerTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // Returns the contents of `zip_file` as a map [filename -> file contents].
  std::map<std::string, std::string> ReadZipFileContents(
      base::FilePath zip_file) {
    const int64_t kMaxEntrySize = 10 * 1024;
    std::map<std::string, std::string> result;
    zip::ZipReader reader;
    if (!reader.Open(zip_file)) {
      ADD_FAILURE() << "Could not open " << zip_file;
      return {};
    }
    while (reader.HasMore()) {
      if (!reader.OpenCurrentEntryInZip()) {
        ADD_FAILURE() << "Could not open entry in zip";
        return {};
      }
      const zip::ZipReader::EntryInfo* entry = reader.current_entry_info();
      std::string entry_file_name =
          entry->file_path().BaseName().MaybeAsASCII();
      if (entry->original_size() > kMaxEntrySize) {
        ADD_FAILURE() << "Zip entry " << entry_file_name
                      << " was too large: " << entry->original_size();
        return {};
      }

      std::string entry_contents;
      if (!reader.ExtractCurrentEntryToString(kMaxEntrySize, &entry_contents)) {
        ADD_FAILURE() << "Can't read zip entry " << entry_file_name
                      << " contents into string";
        return {};
      }
      result[entry_file_name] = entry_contents;

      if (!reader.AdvanceToNextEntry()) {
        ADD_FAILURE() << "Could not advance to next entry";
        return {};
      }
    }
    return result;
  }

 protected:
  base::FilePath GetPathForOutput() { return temp_dir_.GetPath(); }

 private:
  // The temporary directory that we'll store the output files.
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment;
};

TEST_F(SupportToolHandlerTest, CollectSupportData) {
  // Set-up SupportToolHandler and add DataCollectors.
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_1", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_2", false));

  // Collect support data.
  base::test::TestFuture<PIIMap, std::set<SupportToolError>> test_future;
  handler->CollectSupportData(
      test_future.GetCallback<const PIIMap&, std::set<SupportToolError>>());
  const PIIMap& detected_pii = test_future.Get<0>();
  std::set<SupportToolError> errors = test_future.Get<1>();

  // Check if the detected PII map returned from the SupportToolHandler is
  // empty.
  EXPECT_FALSE(detected_pii.empty());
  // Check if the error message returned is empty.
  EXPECT_TRUE(errors.empty());
}

TEST_F(SupportToolHandlerTest, ExportSupportDataTest) {
  // Set-up SupportToolHandler and add DataCollectors.
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_1", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_2", false));

  // Export collected data into a file in temporary directory.
  base::FilePath target_path = GetPathForOutput().Append(
      FILE_PATH_LITERAL("support-tool-export-success"));
  base::test::TestFuture<std::set<SupportToolError>> test_future;
  std::set<PIIType> pii_types{PIIType::kUIHierarchyWindowTitles};
  handler->ExportCollectedData(pii_types, target_path,
                               test_future.GetCallback());
  std::set<SupportToolError> errors = test_future.Get();
  EXPECT_TRUE(errors.empty());

  // SupportToolHandler will achieve the data into a .zip archieve.
  target_path = target_path.AddExtension(FILE_PATH_LITERAL(".zip"));
  EXPECT_TRUE(base::PathExists(target_path));
  // Read the output file contents.
  std::map<std::string, std::string> zip_contents =
      ReadZipFileContents(target_path);
  // Each TestDataCollector should write the output contents on a file in the
  // .zip file which has a same name as the data collector.
  EXPECT_THAT(zip_contents,
              UnorderedElementsAre(
                  Pair("test_data_collector_1", kTestDataToWriteOnFile),
                  Pair("test_data_collector_2", kTestDataToWriteOnFile)));
}

TEST_F(SupportToolHandlerTest, ErrorMessageOnCollectData) {
  // Set-up SupportToolHandler and add DataCollectors.
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_1", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_2", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_3", true));

  base::test::TestFuture<PIIMap, std::set<SupportToolError>> test_future;
  handler->CollectSupportData(
      test_future.GetCallback<const PIIMap&, std::set<SupportToolError>>());
  const PIIMap& detected_pii = test_future.Get<0>();
  std::set<SupportToolError> errors = test_future.Get<1>();

  // Check if the detected PII map returned from the SupportToolHandler is
  // empty.
  EXPECT_FALSE(detected_pii.empty());
  // Check if the error message returned contains the expected result.
  EXPECT_FALSE(errors.empty());
  // SupportToolError::kTestDataCollectorError should be present in the errors
  // returned.
  size_t expected_size = 1;
  EXPECT_EQ(errors.size(), expected_size);
  EXPECT_NE(errors.find(SupportToolError::kTestDataCollectorError),
            errors.end());
}

TEST_F(SupportToolHandlerTest, ErrorMessageOnExportSupportData) {
  // Set-up SupportToolHandler and add DataCollectors.
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>();
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_1", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_2", false));
  handler->AddDataCollector(
      std::make_unique<TestDataCollector>("test_data_collector_3", true));

  // Create the path inside a temporary directory to store the output files.
  base::FilePath target_path =
      GetPathForOutput().Append(FILE_PATH_LITERAL("support-tool-export-error"));

  // Export collected data into the target temporary directory.
  base::test::TestFuture<std::set<SupportToolError>> test_future;
  std::set<PIIType> pii_types{PIIType::kUIHierarchyWindowTitles};
  handler->ExportCollectedData(pii_types, target_path,
                               test_future.GetCallback());

  // Check the error message.
  std::set<SupportToolError> errors = test_future.Get();
  EXPECT_FALSE(errors.empty());
  // SupportToolError::kTestDataCollectorError should be present in the errors
  // returned.
  size_t expected_size = 1;
  EXPECT_EQ(errors.size(), expected_size);
  EXPECT_NE(errors.find(SupportToolError::kTestDataCollectorError),
            errors.end());

  // SupportToolHandler will archive the data into a .zip archive.
  target_path = target_path.AddExtension(FILE_PATH_LITERAL(".zip"));
  EXPECT_TRUE(base::PathExists(target_path));
  // Read the output file contents.
  std::map<std::string, std::string> zip_contents =
      ReadZipFileContents(target_path);
  // Each TestDataCollector should write the output contents on a file in the
  // .zip file which has a same name as the data collector. The data collectors
  // with error won't create and write to any file.
  EXPECT_THAT(zip_contents,
              UnorderedElementsAre(
                  Pair("test_data_collector_1", kTestDataToWriteOnFile),
                  Pair("test_data_collector_2", kTestDataToWriteOnFile)));
}
