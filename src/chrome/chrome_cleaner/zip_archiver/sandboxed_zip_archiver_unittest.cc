// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/interfaces/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/sandboxed_zip_archiver.h"
#include "chrome/chrome_cleaner/zip_archiver/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/test_zip_archiver_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

constexpr char kTestPassword[] = "1234";
constexpr char kTestSymlink[] = "a.link";
constexpr uint32_t kProhibitedAccessPermissions[] = {
    DELETE,       READ_CONTROL,     WRITE_DAC,
    WRITE_OWNER,  FILE_APPEND_DATA, FILE_EXECUTE,
    FILE_READ_EA, FILE_WRITE_EA,    FILE_WRITE_ATTRIBUTES};

class ZipArchiverSandboxedArchiverTest : public base::MultiProcessTest {
 public:
  void SetUp() override {
    scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
    ZipArchiverSandboxSetupHooks setup_hooks(
        mojo_task_runner.get(), base::BindOnce([] {
          FAIL() << "ZipArchiver sandbox connection error";
        }));
    ASSERT_EQ(RESULT_CODE_SUCCESS,
              StartSandboxTarget(MakeCmdLine("SandboxedZipArchiverTargetMain"),
                                 &setup_hooks, SandboxType::kTest));
    UniqueZipArchiverPtr zip_archiver_ptr = setup_hooks.TakeZipArchiverPtr();

    test_file_.Initialize();
    const base::FilePath& src_file_path = test_file_.GetSourceFilePath();

    std::string src_file_hash;
    ComputeSHA256DigestOfPath(src_file_path, &src_file_hash);

    const base::FilePath& dst_archive_folder = test_file_.GetTempDirPath();
    base::FilePath zip_filename(
        base::StrCat({test_file_.GetSourceFilePath().BaseName().AsUTF16Unsafe(),
                      L"_", base::UTF8ToUTF16(src_file_hash), L".zip"}));

    expect_zip_file_path_ = dst_archive_folder.Append(zip_filename);

    zip_archiver_ = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner, std::move(zip_archiver_ptr), dst_archive_folder,
        kTestPassword);
  }

 protected:
  std::unique_ptr<SandboxedZipArchiver> zip_archiver_;
  ZipArchiverTestFile test_file_;
  base::FilePath expect_zip_file_path_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

// |ArchiverPermissionCheckerImpl| runs and handles |Archive| requests in the
// sandbox child process. It checks if the parameters passed in the sandbox are
// configured correctly. It doesn't do real archiving.
class ArchiverPermissionCheckerImpl : public mojom::ZipArchiver {
 public:
  explicit ArchiverPermissionCheckerImpl(mojom::ZipArchiverRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(base::BindOnce(
        [] { FAIL() << "ZipArchiver sandbox connection error"; }));
  }

  ~ArchiverPermissionCheckerImpl() override = default;

  void Archive(mojo::ScopedHandle src_file_handle,
               mojo::ScopedHandle zip_file_handle,
               const std::string& filename,
               const std::string& password,
               ArchiveCallback callback) override {
    HANDLE raw_src_file_handle;
    if (mojo::UnwrapPlatformFile(std::move(src_file_handle),
                                 &raw_src_file_handle) != MOJO_RESULT_OK) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }
    base::File src_file(raw_src_file_handle);
    if (!src_file.IsValid()) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    HANDLE raw_zip_file_handle;
    if (mojo::UnwrapPlatformFile(std::move(zip_file_handle),
                                 &raw_zip_file_handle) != MOJO_RESULT_OK) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }
    base::File zip_file(raw_zip_file_handle);
    if (!zip_file.IsValid()) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    // Test general prohibited file access permissions.
    for (uint32_t permission : kProhibitedAccessPermissions) {
      if (HasPermission(src_file, permission) ||
          HasPermission(zip_file, permission)) {
        std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
        return;
      }
    }

    // Check if |src_file| and |zip_file| have incorrect file access
    // permissions.
    if (HasPermission(src_file, FILE_WRITE_DATA) ||
        HasPermission(zip_file, FILE_READ_DATA)) {
      std::move(callback).Run(ZipArchiverResultCode::kErrorInvalidParameter);
      return;
    }

    std::move(callback).Run(ZipArchiverResultCode::kSuccess);
  }

 private:
  static bool HasPermission(const base::File& file, uint32_t permission) {
    HANDLE temp_handle;
    if (::DuplicateHandle(::GetCurrentProcess(), file.GetPlatformFile(),
                          ::GetCurrentProcess(), &temp_handle, permission,
                          false, 0)) {
      CloseHandle(temp_handle);
      return true;
    }
    return false;
  }

  mojo::Binding<mojom::ZipArchiver> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArchiverPermissionCheckerImpl);
};

}  // namespace

MULTIPROCESS_TEST_MAIN(SandboxedZipArchiverTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  // |RunZipArchiverSandboxTarget| won't return. The mojo error handler will
  // abort this process when the connection is broken.
  RunZipArchiverSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                              sandbox_target_services);

  return 0;
}

TEST_F(ZipArchiverSandboxedArchiverTest, Archive) {
  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(test_file_.GetSourceFilePath(),
                                   &output_zip_file_path),
            ZipArchiverResultCode::kSuccess);

  EXPECT_EQ(output_zip_file_path, expect_zip_file_path_);
  test_file_.ExpectValidZipFile(
      expect_zip_file_path_,
      test_file_.GetSourceFilePath().BaseName().AsUTF8Unsafe(), kTestPassword);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceFileNotFound) {
  ASSERT_TRUE(base::DeleteFile(test_file_.GetSourceFilePath(), false));

  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(test_file_.GetSourceFilePath(),
                                   &output_zip_file_path),
            ZipArchiverResultCode::kErrorCannotOpenSourceFile);
}

TEST_F(ZipArchiverSandboxedArchiverTest, ZipFileExists) {
  base::File zip_file(expect_zip_file_path_, base::File::FLAG_CREATE);
  ASSERT_TRUE(zip_file.IsValid());

  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(test_file_.GetSourceFilePath(),
                                   &output_zip_file_path),
            ZipArchiverResultCode::kZipFileExists);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsSymbolicLink) {
  base::FilePath symlink_path =
      test_file_.GetTempDirPath().AppendASCII(kTestSymlink);
  ASSERT_TRUE(::CreateSymbolicLink(
      symlink_path.AsUTF16Unsafe().c_str(),
      test_file_.GetSourceFilePath().AsUTF16Unsafe().c_str(), 0));

  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(symlink_path, &output_zip_file_path),
            ZipArchiverResultCode::kIgnoredSourceFile);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsDirectory) {
  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(test_file_.GetTempDirPath(),
                                   &output_zip_file_path),
            ZipArchiverResultCode::kIgnoredSourceFile);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsDefaultFileStream) {
  base::FilePath stream_path(base::StrCat(
      {test_file_.GetSourceFilePath().AsUTF16Unsafe(), L"::$data"}));

  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(stream_path, &output_zip_file_path),
            ZipArchiverResultCode::kSuccess);

  EXPECT_EQ(output_zip_file_path, expect_zip_file_path_);
  test_file_.ExpectValidZipFile(
      expect_zip_file_path_,
      test_file_.GetSourceFilePath().BaseName().AsUTF8Unsafe(), kTestPassword);
}

TEST_F(ZipArchiverSandboxedArchiverTest, SourceIsNonDefaultFileStream) {
  base::FilePath stream_path(base::StrCat(
      {test_file_.GetSourceFilePath().AsUTF16Unsafe(), L":stream:$data"}));
  base::File stream_file(stream_path, base::File::FLAG_CREATE);
  ASSERT_TRUE(stream_file.IsValid());

  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(stream_path, &output_zip_file_path),
            ZipArchiverResultCode::kIgnoredSourceFile);
}

namespace {

// |ZipArchiverIsolationTest| uses |ArchiverPermissionCheckerImpl| to check the
// sandbox configuration.
class ZipArchiverIsolationTest : public base::MultiProcessTest {
 public:
  ZipArchiverIsolationTest()
      : mojo_task_runner_(MojoTaskRunner::Create()),
        impl_ptr_(nullptr, base::OnTaskRunnerDeleter(mojo_task_runner_)) {
    UniqueZipArchiverPtr zip_archiver_ptr(
        new mojom::ZipArchiverPtr(),
        base::OnTaskRunnerDeleter(mojo_task_runner_));

    // Initialize the |impl_ptr_| in the mojo task and wait until it completed.
    base::RunLoop loop;
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ZipArchiverIsolationTest::InitializeArchiverPermissionCheckerImpl,
            base::Unretained(this), zip_archiver_ptr.get(),
            loop.QuitClosure()));
    loop.Run();

    test_file_.Initialize();

    zip_archiver_ = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner_, std::move(zip_archiver_ptr),
        test_file_.GetTempDirPath(), kTestPassword);
  }

 protected:
  std::unique_ptr<SandboxedZipArchiver> zip_archiver_;
  ZipArchiverTestFile test_file_;

 private:
  void InitializeArchiverPermissionCheckerImpl(
      mojom::ZipArchiverPtr* zip_archiver_ptr,
      base::OnceClosure callback) {
    impl_ptr_.reset(
        new ArchiverPermissionCheckerImpl(mojo::MakeRequest(zip_archiver_ptr)));
    std::move(callback).Run();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  std::unique_ptr<ArchiverPermissionCheckerImpl, base::OnTaskRunnerDeleter>
      impl_ptr_;
};

}  // namespace

TEST_F(ZipArchiverIsolationTest, CheckPermission) {
  base::FilePath output_zip_file_path;
  EXPECT_EQ(zip_archiver_->Archive(test_file_.GetSourceFilePath(),
                                   &output_zip_file_path),
            ZipArchiverResultCode::kSuccess);
}

}  // namespace chrome_cleaner
