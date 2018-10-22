// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_remover.h"

#include <stdint.h>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/whitelisted_directory.h"
#include "chrome/chrome_cleaner/test/reboot_deletion_helper.h"
#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_layered_service_provider.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::Eq;
using testing::Return;
using ValidationStatus = FileRemoverAPI::DeletionValidationStatus;

const wchar_t kRemoveFile1[] = L"remove_one.exe";
const wchar_t kRemoveFile2[] = L"remove_two.exe";
const wchar_t kRemoveFolder[] = L"remove";

class FileRemoverTest : public ::testing::Test {
 protected:
  FileRemoverTest()
      : default_file_remover_(
            nullptr,
            LayeredServiceProviderWrapper(),
            /*deletion_allowed_paths=*/{},
            base::BindRepeating(&FileRemoverTest::RebootRequired,
                                base::Unretained(this))) {
    FileRemovalStatusUpdater::GetInstance()->Clear();
  }

  void RebootRequired() { reboot_required_ = true; }

  void TestBlacklistedRemoval(FileRemover* remover,
                              const base::FilePath& path) {
    DCHECK(remover);

    EXPECT_EQ(ValidationStatus::FORBIDDEN, remover->CanRemove(path));

    FileRemovalStatusUpdater* removal_status_updater =
        FileRemovalStatusUpdater::GetInstance();

    EXPECT_FALSE(remover->RemoveNow(path));
    EXPECT_EQ(removal_status_updater->GetRemovalStatus(path),
              REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);

    removal_status_updater->Clear();
    EXPECT_FALSE(remover->RegisterPostRebootRemoval(path));
    EXPECT_EQ(removal_status_updater->GetRemovalStatus(path),
              REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);

    EXPECT_TRUE(base::PathExists(path));
    EXPECT_FALSE(IsFileRegisteredForPostRebootRemoval(path));
  }

  FileRemover default_file_remover_;
  bool reboot_required_ = false;
};

}  // namespace

TEST_F(FileRemoverTest, RemoveNowValidFile) {
  // Create a temporary empty file.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  const base::FilePath file_path = temp.GetPath().Append(kRemoveFile1);
  EXPECT_TRUE(CreateEmptyFile(file_path));

  // Removing it must succeed.
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path));
  EXPECT_FALSE(base::PathExists(file_path));
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(file_path),
      REMOVAL_STATUS_REMOVED);
}

TEST_F(FileRemoverTest, RemoveNowAbsentFile) {
  // Create a non-existing file name.
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  const base::FilePath file_path = temp.GetPath().Append(kRemoveFile1);
  EXPECT_FALSE(base::PathExists(file_path));

  // Removing it must not generate an error.
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_NOT_FOUND);

  // Ensure the non-existant files with non-existant parents don't generate an
  // error.
  base::FilePath file_path_deeper = file_path.Append(kRemoveFile2);
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path_deeper));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path_deeper),
            REMOVAL_STATUS_NOT_FOUND);
}

TEST_F(FileRemoverTest, NoKnownFileRemoval) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  FileRemover remover(
      DigestVerifier::CreateFromResource(IDS_TEST_SAMPLE_DLL_DIGEST),
      LayeredServiceProviderWrapper(), /*deletion_allowed_paths=*/{},
      base::DoNothing::Repeatedly());

  // Copy the sample DLL to the temp folder.
  base::FilePath dll_path = GetSampleDLLPath();
  ASSERT_TRUE(base::PathExists(dll_path)) << dll_path.value();

  base::FilePath target_dll_path(
      temp_dir.GetPath().Append(dll_path.BaseName()));
  ASSERT_TRUE(base::CopyFile(dll_path, target_dll_path));

  TestBlacklistedRemoval(&remover, target_dll_path);
}

TEST_F(FileRemoverTest, NoSelfRemoval) {
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  TestBlacklistedRemoval(&default_file_remover_, exe_path);
}

TEST_F(FileRemoverTest, NoWhitelistedFileRemoval) {
  base::FilePath program_files_dir =
      PreFetchedPaths::GetInstance()->GetProgramFilesFolder();
  TestBlacklistedRemoval(&default_file_remover_, program_files_dir);
}

TEST_F(FileRemoverTest, NoWhitelistFileTempRemoval) {
  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  TestBlacklistedRemoval(&default_file_remover_, temp_dir);
}

TEST_F(FileRemoverTest, NoLSPRemoval) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath provider_path = temp.GetPath().Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(provider_path));

  TestLayeredServiceProvider lsp;
  lsp.AddProvider(kGUID1, provider_path);

  FileRemover remover(nullptr, lsp, /*deletion_allowed_paths=*/{},
                      base::DoNothing::Repeatedly());

  TestBlacklistedRemoval(&remover, provider_path);
}

TEST_F(FileRemoverTest, CanRemoveAbsolutePath) {
  EXPECT_EQ(ValidationStatus::ALLOWED,
            default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\bar")));
}

TEST_F(FileRemoverTest, NoRelativePathRemoval) {
  EXPECT_EQ(ValidationStatus::FORBIDDEN,
            default_file_remover_.CanRemove(base::FilePath(L"bar.txt")));
}

TEST_F(FileRemoverTest, NoDriveRemoval) {
  EXPECT_EQ(ValidationStatus::FORBIDDEN,
            default_file_remover_.CanRemove(base::FilePath(L"C:")));
  EXPECT_EQ(ValidationStatus::FORBIDDEN,
            default_file_remover_.CanRemove(base::FilePath(L"C:\\")));
}

TEST_F(FileRemoverTest, NoPathTraversal) {
  EXPECT_EQ(
      ValidationStatus::FORBIDDEN,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\..\\bar")));
  EXPECT_EQ(
      ValidationStatus::FORBIDDEN,
      default_file_remover_.CanRemove(base::FilePath(L"..\\foo\\bar.dll")));
}

TEST_F(FileRemoverTest, CorrectPathTraversalDetection) {
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\..bar.dll")));
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo\\bar..dll")));
  EXPECT_EQ(
      ValidationStatus::ALLOWED,
      default_file_remover_.CanRemove(base::FilePath(L"C:\\foo..\\bar.dll")));
}

TEST_F(FileRemoverTest, RemoveNowDoesNotDeleteFolders) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create the folder and the files.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  base::CreateDirectory(subfolder_path);
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));

  // The folder should not be removed.
  EXPECT_FALSE(default_file_remover_.RemoveNow(subfolder_path));
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(subfolder_path),
      REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
  EXPECT_TRUE(base::PathExists(subfolder_path));
  EXPECT_TRUE(base::PathExists(file_path1));
}

TEST_F(FileRemoverTest, RemoveNowDeletesEmptyFolders) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create the folder and the files.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  base::CreateDirectory(subfolder_path);
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));
  base::FilePath subsubfolder_path = subfolder_path.Append(kRemoveFolder);
  base::CreateDirectory(subsubfolder_path);
  base::FilePath file_path2 = subsubfolder_path.Append(kRemoveFile2);
  ASSERT_TRUE(CreateEmptyFile(file_path2));

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Delete a file in a folder with other stuff, so the folder isn't deleted.
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path1));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path1),
            REMOVAL_STATUS_REMOVED);
  EXPECT_TRUE(base::PathExists(subfolder_path));
  EXPECT_FALSE(base::PathExists(file_path1));
  EXPECT_TRUE(base::PathExists(subsubfolder_path));
  EXPECT_TRUE(base::PathExists(file_path2));

  // Delete the file and ensure the two parent folders are deleted since they
  // are now empty.
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path2));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path2),
            REMOVAL_STATUS_REMOVED);
  EXPECT_FALSE(base::PathExists(subfolder_path));
  EXPECT_FALSE(base::PathExists(subsubfolder_path));
  EXPECT_FALSE(base::PathExists(file_path2));
}

TEST_F(FileRemoverTest, RemoveNowDeletesEmptyFoldersNotTemp) {
  base::ScopedPathOverride temp_override(base::DIR_TEMP);

  base::FilePath scoped_temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &scoped_temp_dir));

  // Create a file in temp.
  base::FilePath file_path = scoped_temp_dir.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path));

  // Delete the file and ensure Temp isn't deleted since it is whitelisted.
  EXPECT_TRUE(default_file_remover_.RemoveNow(file_path));
  EXPECT_EQ(
      FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(file_path),
      REMOVAL_STATUS_REMOVED);
  EXPECT_FALSE(base::PathExists(file_path));
  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  EXPECT_TRUE(base::PathExists(temp_dir));
}

TEST_F(FileRemoverTest, RegisterPostRebootRemoval) {
  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // When trying to delete a whitelisted file, we should fail to register the
  // file for removal, and no reboot should be required.
  base::FilePath exe_path = PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_FALSE(default_file_remover_.RegisterPostRebootRemoval(exe_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(exe_path),
            REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
  EXPECT_FALSE(reboot_required_);

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(kRemoveFile2);

  // When trying to delete an non-existant file, we should return success, but
  // not require a reboot.
  EXPECT_TRUE(default_file_remover_.RegisterPostRebootRemoval(file_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_NOT_FOUND);
  EXPECT_FALSE(reboot_required_);

  // When trying to delete a real file, we should return success and require a
  // reboot.
  ASSERT_TRUE(CreateEmptyFile(file_path));
  EXPECT_TRUE(default_file_remover_.RegisterPostRebootRemoval(file_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(file_path),
            REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  EXPECT_TRUE(reboot_required_);
  EXPECT_TRUE(IsFileRegisteredForPostRebootRemoval(file_path));
}

TEST_F(FileRemoverTest, RegisterPostRebootRemoval_Directories) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create an empty directory.
  base::FilePath subfolder_path = temp.GetPath().Append(kRemoveFolder);
  ASSERT_TRUE(base::CreateDirectory(subfolder_path));

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Directories shouldn't be registered for deletion.
  EXPECT_FALSE(default_file_remover_.RegisterPostRebootRemoval(subfolder_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(subfolder_path),
            REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);

  // Put a file into the directory and ensure the non-empty directory still
  // isn't registered for removal.
  removal_status_updater->Clear();
  base::FilePath file_path1 = subfolder_path.Append(kRemoveFile1);
  ASSERT_TRUE(CreateEmptyFile(file_path1));
  EXPECT_FALSE(default_file_remover_.RegisterPostRebootRemoval(subfolder_path));
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(subfolder_path),
            REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL);
}

TEST_F(FileRemoverTest, NotActiveFileType) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath path = temp.GetPath().Append(L"temp_file.txt");
  ASSERT_TRUE(
      CreateFileInFolder(path.DirName(), path.BaseName().value().c_str()));

  EXPECT_EQ(ValidationStatus::INACTIVE,
            FileRemover::IsFileRemovalAllowed(path, {}, {}));
  EXPECT_TRUE(default_file_remover_.RemoveNow(path));
  EXPECT_EQ(REMOVAL_STATUS_NOT_REMOVED_INACTIVE_EXTENSION,
            FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(FileRemoverTest, NotActiveFileAllowed) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath path = temp.GetPath().Append(L"temp_file.txt");
  ASSERT_TRUE(
      CreateFileInFolder(path.DirName(), path.BaseName().value().c_str()));

  FileRemover remover(nullptr, LayeredServiceProviderWrapper(),
                      {path.value().c_str()}, base::DoNothing::Repeatedly());

  EXPECT_EQ(ValidationStatus::ALLOWED, FileRemover::IsFileRemovalAllowed(
                                           path, {path.value().c_str()}, {}));
  EXPECT_TRUE(remover.RemoveNow(path));
  EXPECT_EQ(REMOVAL_STATUS_REMOVED,
            FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
  EXPECT_FALSE(base::PathExists(path));
}

TEST_F(FileRemoverTest, DosMzExecutable) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath path = temp.GetPath().Append(L"temp_file.txt");
  constexpr char kMzExecutable[] = "MZ executable";
  CreateFileWithContent(path, kMzExecutable, sizeof(kMzExecutable));
  ASSERT_TRUE(base::PathExists(path));

  FileRemover remover(nullptr, LayeredServiceProviderWrapper(),
                      {path.value().c_str()}, base::DoNothing::Repeatedly());

  EXPECT_EQ(ValidationStatus::ALLOWED,
            FileRemover::IsFileRemovalAllowed(path, {}, {}));
  EXPECT_TRUE(remover.RemoveNow(path));
  EXPECT_EQ(REMOVAL_STATUS_REMOVED,
            FileRemovalStatusUpdater::GetInstance()->GetRemovalStatus(path));
  EXPECT_FALSE(base::PathExists(path));
}

}  // namespace chrome_cleaner
