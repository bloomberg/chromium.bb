// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/login_screen_ui/login_screen_extension_ui_handler.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_window.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace {

const char kExtensionName[] = "extension-name";
const char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
const char kResourcePath[] = "path/to/file.html";

}  // namespace

namespace chromeos {

using LoginScreenExtensionUiDialogDelegateUnittest = testing::Test;

TEST_F(LoginScreenExtensionUiDialogDelegateUnittest, Test) {
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(kExtensionName).SetID(kExtensionId).Build();

  base::RunLoop close_callback_wait;

  LoginScreenExtensionUiWindow::CreateOptions create_options(
      extension->short_name(), extension->GetResourceURL(kResourcePath),
      false /*can_be_closed_by_user*/, close_callback_wait.QuitClosure());

  // |delegate| will delete itself when calling |OnDialogClosed()| at the end of
  // the test.
  LoginScreenExtensionUiDialogDelegate* delegate =
      new LoginScreenExtensionUiDialogDelegate(&create_options);

  EXPECT_EQ(ui::MODAL_TYPE_WINDOW, delegate->GetDialogModalType());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_LOGIN_EXTENSION_UI_DIALOG_TITLE,
                                       base::UTF8ToUTF16(kExtensionName)),
            delegate->GetDialogTitle());
  EXPECT_EQ(extensions::Extension::GetResourceURL(
                extensions::Extension::GetBaseURLFromExtensionId(kExtensionId),
                kResourcePath),
            delegate->GetDialogContentURL());
  EXPECT_FALSE(delegate->CanResizeDialog());
  EXPECT_TRUE(delegate->ShouldShowDialogTitle());

  EXPECT_FALSE(delegate->CanCloseDialog());
  delegate->set_can_close(true);
  EXPECT_TRUE(delegate->CanCloseDialog());

  delegate->OnDialogClosed(std::string());
  close_callback_wait.Run();
}

}  // namespace chromeos
