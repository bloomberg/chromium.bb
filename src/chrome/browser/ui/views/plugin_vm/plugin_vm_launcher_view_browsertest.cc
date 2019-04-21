// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_launcher_view.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

const char kZipFile[] = "/downloads/a_zip_file.zip";
const char kZippedFile[] = "a_file.txt";
const char kZipFileHash[] =
    "bb077522e6c6fec07cf863ca44d5701935c4bc36ed12ef154f4cc22df70aec18";
const char kNonMatchingHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kJpgFile[] = "/downloads/image.jpg";
const char kJpgFileHash[] =
    "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";

}  // namespace

class PluginVmLauncherViewForTesting : public PluginVmLauncherView {
 public:
  explicit PluginVmLauncherViewForTesting(Profile* profile)
      : PluginVmLauncherView(profile) {}

  void AddSetupIsFinishedCallbackForTesting(base::RepeatingClosure callback) {
    setup_is_finished_callback_for_testing_ = callback;
  }

 private:
  base::RepeatingClosure setup_is_finished_callback_for_testing_;

  void OnStateUpdated() override {
    PluginVmLauncherView::OnStateUpdated();

    if (state_ == State::FINISHED || state_ == State::ERROR) {
      if (setup_is_finished_callback_for_testing_)
        setup_is_finished_callback_for_testing_.Run();
    }
  }
};

class PluginVmLauncherViewBrowserTest : public DialogBrowserTest {
 public:
  class SetupObserver {
   public:
    void OnSetupFinished() {
      if (closure_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(closure_));
      }
    }

    void WaitForSetupToFinish() {
      base::RunLoop run_loop;
      closure_ = run_loop.QuitClosure();
      run_loop.Run();

      content::RunAllTasksUntilIdle();
    }

   private:
    base::OnceClosure closure_;
  };

  PluginVmLauncherViewBrowserTest() {}

  void SetUp() override { DialogBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    view_ = new PluginVmLauncherViewForTesting(browser()->profile());
    setup_observer_ = new SetupObserver();
    view_->AddSetupIsFinishedCallbackForTesting(base::BindRepeating(
        &SetupObserver::OnSetupFinished, base::Unretained(setup_observer_)));
    views::DialogDelegate::CreateDialogWidget(view_, nullptr, nullptr);
  }

 protected:
  PluginVmLauncherViewForTesting* view_;
  SetupObserver* setup_observer_;

  bool HasAcceptButton() {
    return view_->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return view_->GetDialogClientView()->cancel_button() != nullptr;
  }

  void CheckSetupFailed() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_RETRY_BUTTON));
    EXPECT_EQ(view_->GetBigMessage(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_ERROR_TITLE));

    base::FilePath plugin_vm_image_dir =
        browser()
            ->profile()
            ->GetPath()
            .AppendASCII(plugin_vm::kCrosvmDir)
            .AppendASCII(plugin_vm::kPvmDir)
            .AppendASCII(plugin_vm::kPluginVmImageDir);
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::DirectoryExists(plugin_vm_image_dir));
  }

  void CheckSetupIsFinishedSuccessfully() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_FALSE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_LAUNCH_BUTTON));
    EXPECT_EQ(view_->GetBigMessage(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_FINISHED_TITLE));

    base::FilePath plugin_vm_image_dir =
        browser()
            ->profile()
            ->GetPath()
            .AppendASCII(plugin_vm::kCrosvmDir)
            .AppendASCII(plugin_vm::kPvmDir)
            .AppendASCII(plugin_vm::kPluginVmImageDir);
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::DirectoryExists(plugin_vm_image_dir));
    EXPECT_TRUE(base::PathExists(plugin_vm_image_dir.AppendASCII(kZippedFile)));
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(browser()->profile()->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmLauncherViewBrowserTest);
};

// Test the dialog is actually can be launched.
IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       SetupShouldFinishSuccessfully) {
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kZipFileHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  setup_observer_->WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       SetupShouldFailAsHashesDoNotMatch) {
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  setup_observer_->WaitForSetupToFinish();

  CheckSetupFailed();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       SetupShouldFailAsUnzippingFails) {
  SetPluginVmImagePref(embedded_test_server()->GetURL(kJpgFile).spec(),
                       kJpgFileHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  setup_observer_->WaitForSetupToFinish();

  CheckSetupFailed();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       CouldRetryAfterFailedSetup) {
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  setup_observer_->WaitForSetupToFinish();

  CheckSetupFailed();

  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kZipFileHash);

  // Retry button clicked to retry the download.
  view_->GetDialogClientView()->AcceptWindow();

  setup_observer_->WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();
}
