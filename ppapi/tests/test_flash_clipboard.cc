// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_clipboard.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashClipboard);

TestFlashClipboard::TestFlashClipboard(TestingInstance* instance)
    : TestCase(instance),
      clipboard_interface_(NULL) {
}

bool TestFlashClipboard::Init() {
  clipboard_interface_ = static_cast<const PPB_Flash_Clipboard*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FLASH_CLIPBOARD_INTERFACE));
  return !!clipboard_interface_;
}

void TestFlashClipboard::RunTest() {
  RUN_TEST(ReadWrite);
}

std::string TestFlashClipboard::TestReadWrite() {
  std::string input_str("Hello, world");
  pp::Var input_var(input_str);
  clipboard_interface_->WritePlainText(instance_->pp_instance(),
                                       PP_FLASH_CLIPBOARD_TYPE_STANDARD,
                                       input_var.pp_var());

  // WritePlainText() causes an async request sent to the browser process.
  // As a result, the string written may not be reflected by IsFormatAvailable()
  // or ReadPlainText() immediately. We need to wait and retry.
  const int kIntervalMs = 250;
  const int kMaxIntervals = kActionTimeoutMs / kIntervalMs;

  PP_Bool is_available = PP_FALSE;
  for (int i = 0; i < kMaxIntervals; ++i) {
    is_available = clipboard_interface_->IsFormatAvailable(
        instance_->pp_instance(),
        PP_FLASH_CLIPBOARD_TYPE_STANDARD,
        PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT);
    if (is_available)
      break;

    PlatformSleep(kIntervalMs);
  }
  ASSERT_TRUE(is_available);

  std::string result_str;
  for (int i = 0; i < kMaxIntervals; ++i) {
    pp::Var result_var(pp::Var::PassRef(),
        clipboard_interface_->ReadPlainText(instance_->pp_instance(),
                                            PP_FLASH_CLIPBOARD_TYPE_STANDARD));
    ASSERT_TRUE(result_var.is_string());
    result_str = result_var.AsString();
    if (result_str == input_str)
      break;

    PlatformSleep(kIntervalMs);
  }

  ASSERT_TRUE(result_str == input_str);
  PASS();
}
