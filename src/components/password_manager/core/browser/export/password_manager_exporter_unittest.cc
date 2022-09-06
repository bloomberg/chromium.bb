// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::StrictMock;

// A callback that matches the signature of the StringPiece variant of
// base::WriteFile().
using WriteCallback =
    base::RepeatingCallback<bool(const base::FilePath&, base::StringPiece)>;
using DeleteCallback = PasswordManagerExporter::DeleteCallback;
using SetPosixFilePermissionsCallback =
    PasswordManagerExporter::SetPosixFilePermissionsCallback;

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/nul");
#else
const base::FilePath::CharType kNullFileName[] = FILE_PATH_LITERAL("/dev/null");
#endif

// Creates a hardcoded set of credentials for tests.
PasswordForm CreateTestPassword() {
  PasswordForm password_form;
  password_form.url = GURL("http://accounts.google.com/a/LoginAuth");
  password_form.username_value = u"test@gmail.com";
  password_form.password_value = u"test1";
  password_form.in_store = PasswordForm::Store::kProfileStore;
  return password_form;
}

class PasswordManagerExporterTest : public testing::Test {
 public:
  PasswordManagerExporterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        exporter_(&presenter_, mock_on_progress_.Get()),
        destination_path_(kNullFileName) {
    exporter_.SetWriteForTesting(mock_write_file_.Get());
    exporter_.SetDeleteForTesting(mock_delete_file_.Get());
    exporter_.SetSetPosixFilePermissionsForTesting(
        mock_set_posix_file_permissions_.Get());
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  PasswordManagerExporterTest(const PasswordManagerExporterTest&) = delete;
  PasswordManagerExporterTest& operator=(const PasswordManagerExporterTest&) =
      delete;

  ~PasswordManagerExporterTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  void SetPasswordList(const std::vector<PasswordForm>& forms) {
    for (const auto& form : forms) {
      store_->AddLogin(form);
    }
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  SavedPasswordsPresenter presenter_{store_};
  base::MockCallback<
      base::RepeatingCallback<void(ExportProgressStatus, const std::string&)>>
      mock_on_progress_;
  PasswordManagerExporter exporter_;
  StrictMock<base::MockCallback<WriteCallback>> mock_write_file_;
  StrictMock<base::MockCallback<DeleteCallback>> mock_delete_file_;
  NiceMock<base::MockCallback<SetPosixFilePermissionsCallback>>
      mock_set_posix_file_permissions_;
  base::FilePath destination_path_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PasswordManagerExporterTest, PasswordExportSetPasswordListFirst) {
  PasswordForm form = CreateTestPassword();
  SetPasswordList({form});
  const std::string serialised(
      PasswordCSVWriter::SerializePasswords({CredentialUIEntry(form)}));

  EXPECT_CALL(mock_write_file_, Run(destination_path_, StrEq(serialised)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::SUCCEEDED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

// When writing fails, we should notify the UI of the failure and try to cleanup
// a possibly partial passwords file.
TEST_F(PasswordManagerExporterTest, WriteFileFailed) {
  SetPasswordList({CreateTestPassword()});
  const std::string destination_folder_name(
      destination_path_.DirName().BaseName().AsUTF8Unsafe());

  EXPECT_CALL(mock_write_file_, Run(_, _)).WillOnce(Return(false));
  EXPECT_CALL(mock_delete_file_, Run(destination_path_));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_, Run(ExportProgressStatus::FAILED_WRITE_FAILED,
                                     StrEq(destination_folder_name)));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

// Test that GetProgressStatus() returns the last ExportProgressStatus sent
// to the callback.
TEST_F(PasswordManagerExporterTest, GetProgressReturnsLastCallbackStatus) {
  PasswordForm form = CreateTestPassword();
  SetPasswordList({form});
  const std::string serialised(
      PasswordCSVWriter::SerializePasswords({CredentialUIEntry(form)}));
  const std::string destination_folder_name(
      destination_path_.DirName().BaseName().AsUTF8Unsafe());

  // The last status seen in the callback.
  ExportProgressStatus status = ExportProgressStatus::NOT_STARTED;

  EXPECT_CALL(mock_write_file_, Run(_, _)).WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run(_, _)).WillRepeatedly(SaveArg<0>(&status));

  ASSERT_EQ(exporter_.GetProgressStatus(), status);
  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  ASSERT_EQ(exporter_.GetProgressStatus(), status);

  task_environment_.RunUntilIdle();
  ASSERT_EQ(exporter_.GetProgressStatus(), status);
}

TEST_F(PasswordManagerExporterTest, DontExportWithOnlyDestination) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run(_, _)).Times(0);
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));

  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelAfterPasswords) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run(_, _)).Times(0);
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerExporterTest, CancelWhileExporting) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run(_, _)).Times(0);
  EXPECT_CALL(mock_delete_file_, Run(destination_path_));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

// The "Cancel" button may still be visible on the UI after we've completed
// exporting. If they choose to cancel, we should clear the file.
TEST_F(PasswordManagerExporterTest, CancelAfterExporting) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run(_, _)).WillOnce(Return(true));
  EXPECT_CALL(mock_delete_file_, Run(destination_path_));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::SUCCEEDED, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::FAILED_CANCELLED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
  exporter_.Cancel();

  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(IS_POSIX)
// Chrome creates files using the broadest permissions allowed. Passwords are
// sensitive and should be explicitly limited to the owner.
TEST_F(PasswordManagerExporterTest, OutputHasRestrictedPermissions) {
  SetPasswordList({CreateTestPassword()});

  EXPECT_CALL(mock_write_file_, Run(_, _)).WillOnce(Return(true));
  EXPECT_CALL(mock_set_posix_file_permissions_, Run(destination_path_, 0600))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_, Run(_, _)).Times(AnyNumber());

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}
#endif

TEST_F(PasswordManagerExporterTest, DeduplicatesAcrossPasswordStores) {
  PasswordForm password;
  password.in_store = PasswordForm::Store::kProfileStore;
  password.url = GURL("http://g.com/auth");
  password.username_value = u"user";
  password.password_value = u"password";

  PasswordForm password_duplicate = password;
  password_duplicate.in_store = PasswordForm::Store::kAccountStore;

  const std::string single_password_serialised(
      PasswordCSVWriter::SerializePasswords({CredentialUIEntry(password)}));
  SetPasswordList({password, password_duplicate});

  // The content written to the file should be the same as what would be
  // computed before the duplicated password was added.
  EXPECT_CALL(mock_write_file_,
              Run(destination_path_, StrEq(single_password_serialised)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::IN_PROGRESS, IsEmpty()));
  EXPECT_CALL(mock_on_progress_,
              Run(ExportProgressStatus::SUCCEEDED, IsEmpty()));

  exporter_.PreparePasswordsForExport();
  exporter_.SetDestination(destination_path_);

  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace password_manager
