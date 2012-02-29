// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_clipboard.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashClipboard);

// WriteData() sends an async request to the browser process. As a result, the
// string written may not be reflected by IsFormatAvailable() or ReadPlainText()
// immediately. We need to wait and retry.
const int kIntervalMs = 250;
const int kMaxIntervals = kActionTimeoutMs / kIntervalMs;

TestFlashClipboard::TestFlashClipboard(TestingInstance* instance)
    : TestCase(instance),
      clipboard_interface_(NULL) {
}

bool TestFlashClipboard::Init() {
  clipboard_interface_ = static_cast<const PPB_Flash_Clipboard*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FLASH_CLIPBOARD_INTERFACE));
  return !!clipboard_interface_;
}

void TestFlashClipboard::RunTests(const std::string& filter) {
  RUN_TEST(ReadWritePlainText, filter);
  RUN_TEST(ReadWriteHTML, filter);
  RUN_TEST(ReadWriteMultipleFormats, filter);
  RUN_TEST(Clear, filter);
}

PP_Bool TestFlashClipboard::IsFormatAvailable(
    PP_Flash_Clipboard_Format format) {
  PP_Bool is_available = PP_FALSE;
  for (int i = 0; i < kMaxIntervals; ++i) {
    is_available = clipboard_interface_->IsFormatAvailable(
        instance_->pp_instance(),
        PP_FLASH_CLIPBOARD_TYPE_STANDARD,
        format);
    if (is_available)
      break;

    PlatformSleep(kIntervalMs);
  }
  return is_available;
}

std::string TestFlashClipboard::ReadStringVar(
    PP_Flash_Clipboard_Format format) {
  std::string result_str;
  pp::Var result_var(
      pp::PASS_REF,
      clipboard_interface_->ReadData(instance_->pp_instance(),
                                     PP_FLASH_CLIPBOARD_TYPE_STANDARD,
                                     format));
  if (result_var.is_string())
    result_str = result_var.AsString();
  return result_str;
}

int32_t TestFlashClipboard::WriteStringVar(PP_Flash_Clipboard_Format format,
                                           const std::string& input) {
  pp::Var input_var(input);
  PP_Var data_item = input_var.pp_var();
  int32_t success = clipboard_interface_->WriteData(
      instance_->pp_instance(),
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      1,
      &format,
      &data_item);
  return success;
}

bool TestFlashClipboard::ReadAndMatchPlainText(const std::string& input) {
  for (int i = 0; i < kMaxIntervals; ++i) {
    if (ReadStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) == input) {
      return true;
    }
    PlatformSleep(kIntervalMs);
  }
  return false;
}

bool TestFlashClipboard::ReadAndMatchHTML(const std::string& input) {
  for (int i = 0; i < kMaxIntervals; ++i) {
    std::string result = ReadStringVar(PP_FLASH_CLIPBOARD_FORMAT_HTML);
    // Markup is inserted around the copied html, so just check that
    // the pasted string contains the copied string.
    bool match = result.find(input) != std::string::npos;
    if (match) {
      return true;
    }
    PlatformSleep(kIntervalMs);
  }
  return false;
}

std::string TestFlashClipboard::TestReadWritePlainText() {
  std::string input = "Hello world plain text!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                             input) == PP_OK);
  ASSERT_TRUE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT));
  ASSERT_TRUE(ReadAndMatchPlainText(input));

  PASS();
}

std::string TestFlashClipboard::TestReadWriteHTML() {
  std::string input = "Hello world html!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_HTML,
                             input) == PP_OK);
  ASSERT_TRUE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_HTML));
  ASSERT_TRUE(ReadAndMatchHTML(input));

  PASS();
}

std::string TestFlashClipboard::TestReadWriteMultipleFormats() {
  pp::Var plain_text_var("plain text");
  pp::Var html_var("html");
  PP_Flash_Clipboard_Format formats[] =
      { PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT, PP_FLASH_CLIPBOARD_FORMAT_HTML };
  PP_Var data_items[] = { plain_text_var.pp_var(), html_var.pp_var() };
  int32_t success = clipboard_interface_->WriteData(
      instance_->pp_instance(),
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      sizeof(data_items) / sizeof(*data_items),
      formats,
      data_items);
  ASSERT_TRUE(success == PP_OK);

  ASSERT_TRUE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT));
  ASSERT_TRUE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_HTML));

  ASSERT_TRUE(ReadAndMatchPlainText(plain_text_var.AsString()));
  ASSERT_TRUE(ReadAndMatchHTML(html_var.AsString()));

  PASS();
}

std::string TestFlashClipboard::TestClear() {
  std::string input = "Hello world plain text!";
  ASSERT_TRUE(WriteStringVar(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT,
                             input) == PP_OK);
  ASSERT_TRUE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT));
  clipboard_interface_->WriteData(
      instance_->pp_instance(),
      PP_FLASH_CLIPBOARD_TYPE_STANDARD,
      0,
      NULL,
      NULL);
  ASSERT_FALSE(IsFormatAvailable(PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT));
  PASS();
}
