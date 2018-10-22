// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_cleaner_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const char kFileContent[] = "This is the file content.";

const UwSId kFakePupId1 = 10;
const UwSId kFakePupId2 = 27;

}  // namespace

class PUPCleanerUtilTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir_.GetPath(), L"subfolder",
                                              &subfolder_path_));
  }

  base::FilePath CreateFileInTopDir(const base::string16& basename,
                                    const char* content) {
    base::FilePath file_path = temp_dir_.GetPath().Append(basename);
    CreateFileWithContent(file_path, content, strlen(content));
    return file_path;
  }

  base::FilePath CreateFileInSubfolder(const base::string16& basename,
                                       const char* content) {
    base::FilePath file_path = subfolder_path_.Append(basename);
    CreateFileWithContent(file_path, content, strlen(content));
    return file_path;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath subfolder_path_;
};

TEST_F(PUPCleanerUtilTest, CollectRemovablePupFiles_ActiveFiles) {
  PUPData pup_data;
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId1, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  test_pup_data.AddPUP(kFakePupId2, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);

  base::FilePath active_path1 = CreateFileInTopDir(L"file.exe", kFileContent);
  EXPECT_TRUE(PathHasActiveExtension(active_path1));
  base::FilePath active_path2 =
      CreateFileInSubfolder(L"file.dll", kFileContent);
  EXPECT_TRUE(PathHasActiveExtension(active_path2));
  base::FilePath lnk_path = CreateFileInTopDir(L"file.lnk", kFileContent);
  EXPECT_TRUE(PathHasActiveExtension(lnk_path));

  PUPData::PUP* pup1 = pup_data.GetPUP(kFakePupId1);
  ASSERT_TRUE(pup1);
  PUPData::PUP* pup2 = pup_data.GetPUP(kFakePupId2);
  ASSERT_TRUE(pup2);

  pup1->AddDiskFootprint(active_path1);
  pup1->AddDiskFootprint(lnk_path);
  pup1->AddDiskFootprint(temp_dir_.GetPath());

  pup2->AddDiskFootprint(active_path2);
  pup2->AddDiskFootprint(subfolder_path_);

  FilePathSet expected_collected_paths;
  expected_collected_paths.Insert(active_path1);
  expected_collected_paths.Insert(active_path2);
  expected_collected_paths.Insert(lnk_path);

  FilePathSet collected_paths;
  EXPECT_TRUE(CollectRemovablePupFiles(Engine::URZA, {kFakePupId1, kFakePupId2},
                                       &collected_paths));
  EXPECT_EQ(expected_collected_paths, collected_paths);
}

TEST_F(PUPCleanerUtilTest, CollectRemovablePupFiles_InactiveFiles) {
  // Files with inactive extensions should be collected only if they were
  //  detected as part of UwS service registration.
  PUPData pup_data;
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId1, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);

  base::FilePath inactive_path = CreateFileInTopDir(L"file.jpg", kFileContent);
  EXPECT_FALSE(PathHasActiveExtension(inactive_path));
  base::FilePath found_in_service_path =
      CreateFileInTopDir(L"file.log", kFileContent);
  EXPECT_FALSE(PathHasActiveExtension(found_in_service_path));

  PUPData::PUP* pup = pup_data.GetPUP(kFakePupId1);
  ASSERT_TRUE(pup);
  pup->AddDiskFootprint(inactive_path);
  pup->AddDiskFootprintTraceLocation(inactive_path, UwS::FOUND_IN_MEMORY);
  pup->AddDiskFootprint(found_in_service_path);
  pup->AddDiskFootprintTraceLocation(found_in_service_path,
                                     UwS::FOUND_IN_SERVICE);

  FilePathSet expected_collected_paths;
  expected_collected_paths.Insert(found_in_service_path);

  FilePathSet collected_paths;
  EXPECT_TRUE(
      CollectRemovablePupFiles(Engine::URZA, {kFakePupId1}, &collected_paths));
  EXPECT_EQ(expected_collected_paths, collected_paths);
}

TEST_F(PUPCleanerUtilTest, CollectRemovablePupFiles_TooManyFiles) {
  // CollectRemovablePupFiles() should return false when the number of collected
  // files for a PUP exceed the PUPs max_files_to_remove threshold.
  PUPData pup_data;
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId1, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       /*max_files_to_remove=*/1);
  PUPData::PUP* pup = pup_data.GetPUP(kFakePupId1);
  ASSERT_TRUE(pup);

  base::FilePath active_path1 = CreateFileInTopDir(L"file1.exe", kFileContent);
  EXPECT_TRUE(PathHasActiveExtension(active_path1));
  base::FilePath active_path2 =
      CreateFileInSubfolder(L"file2.exe", kFileContent);
  EXPECT_TRUE(PathHasActiveExtension(active_path2));

  pup->AddDiskFootprint(active_path1);
  pup->AddDiskFootprint(active_path2);

  FilePathSet expected_collected_paths;
  expected_collected_paths.Insert(active_path1);
  expected_collected_paths.Insert(active_path2);

  FilePathSet collected_paths_unsafe;
  EXPECT_FALSE(CollectRemovablePupFiles(Engine::URZA, {kFakePupId1},
                                        &collected_paths_unsafe));
  EXPECT_EQ(expected_collected_paths, collected_paths_unsafe);
}

}  // namespace chrome_cleaner
