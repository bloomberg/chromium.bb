// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_char_set.h"

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(CharSet);

TestCharSet::TestCharSet(TestingInstance* instance)
    : TestCase(instance),
      char_set_interface_(NULL) {
}

bool TestCharSet::Init() {
  char_set_interface_ = static_cast<PPB_CharSet_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CHAR_SET_DEV_INTERFACE));
  return !!char_set_interface_;
}

void TestCharSet::RunTests(const std::string& filter) {
  RUN_TEST(UTF16ToCharSet, filter);
  RUN_TEST(CharSetToUTF16, filter);
  RUN_TEST(GetDefaultCharSet, filter);
}

std::string TestCharSet::TestUTF16ToCharSet() {
  // Empty string.
  std::vector<uint16_t> utf16;
  utf16.push_back(0);
  uint32_t utf8result_len = 0;
  pp::Memory_Dev memory;
  char* utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], 0, "latin1",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result);
  ASSERT_TRUE(utf8result[0] == 0);
  ASSERT_TRUE(utf8result_len == 0);
  memory.MemFree(utf8result);

  // Try round-tripping some English & Chinese from UTF-8 through UTF-16
  std::string utf8source("Hello, world. \xe4\xbd\xa0\xe5\xa5\xbd");
  utf16 = UTF8ToUTF16(utf8source);
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "Utf-8", PP_CHARSET_CONVERSIONERROR_FAIL, &utf8result_len);
  ASSERT_TRUE(utf8source == std::string(utf8result, utf8result_len));
  memory.MemFree(utf8result);

  // Test an un-encodable character with various modes.
  utf16 = UTF8ToUTF16("h\xe4\xbd\xa0i");

  // Fail mode.
  utf8result_len = 1234;  // Test that this gets 0'ed on failure.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_FAIL, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 0);
  ASSERT_TRUE(utf8result == NULL);

  // Skip mode.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SKIP, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(utf8result[0] == 'h' && utf8result[1] == 'i' &&
              utf8result[2] == 0);
  memory.MemFree(utf8result);

  // Substitute mode.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 3);
  ASSERT_TRUE(utf8result[0] == 'h' && utf8result[1] == '?' &&
              utf8result[2] == 'i' && utf8result[3] == 0);
  memory.MemFree(utf8result);

  // Try some invalid input encoding.
  utf16.clear();
  utf16.push_back(0xD800);  // High surrogate.
  utf16.push_back('A');  // Not a low surrogate.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "latin1", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(utf8result_len == 2);
  ASSERT_TRUE(utf8result[0] == '?' && utf8result[1] == 'A' &&
              utf8result[2] == 0);
  memory.MemFree(utf8result);

  // Invalid encoding name.
  utf8result = char_set_interface_->UTF16ToCharSet(
      instance_->pp_instance(), &utf16[0], static_cast<uint32_t>(utf16.size()),
      "poopiepants", PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf8result_len);
  ASSERT_TRUE(!utf8result);
  ASSERT_TRUE(utf8result_len == 0);

  PASS();
}

std::string TestCharSet::TestCharSetToUTF16() {
  pp::Memory_Dev memory;

  // Empty string.
  uint32_t utf16result_len;
  uint16_t* utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), "", 0, "latin1",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  ASSERT_TRUE(utf16result[0] == 0);
  memory.MemFree(utf16result);

  // Basic Latin1.
  char latin1[] = "H\xef";
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), latin1, 2, "latin1",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(utf16result[0] == 'H' && utf16result[1] == 0xef &&
              utf16result[2] == 0);
  memory.MemFree(utf16result);

  // Invalid input encoding with FAIL.
  char badutf8[] = "A\xe4Z";
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_FAIL, &utf16result_len);
  ASSERT_TRUE(!utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  memory.MemFree(utf16result);

  // Invalid input with SKIP.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_SKIP, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 2);
  ASSERT_TRUE(utf16result[0] == 'A' && utf16result[1] == 'Z' &&
              utf16result[2] == 0);
  memory.MemFree(utf16result);

  // Invalid input with SUBSTITUTE.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "utf8",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf16result_len);
  ASSERT_TRUE(utf16result);
  ASSERT_TRUE(utf16result_len == 3);
  ASSERT_TRUE(utf16result[0] == 'A' && utf16result[1] == 0xFFFD &&
              utf16result[2] == 'Z' && utf16result[3] == 0);
  memory.MemFree(utf16result);

  // Invalid encoding name.
  utf16result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), badutf8, 3, "poopiepants",
      PP_CHARSET_CONVERSIONERROR_SUBSTITUTE, &utf16result_len);
  ASSERT_TRUE(!utf16result);
  ASSERT_TRUE(utf16result_len == 0);
  memory.MemFree(utf16result);

  PASS();
}

std::string TestCharSet::TestGetDefaultCharSet() {
  // Test invalid instance.
  pp::Var result(pp::Var::PassRef(), char_set_interface_->GetDefaultCharSet(0));
  ASSERT_TRUE(result.is_undefined());

  // Just make sure the default char set is a nonempty string.
  result = pp::Var(pp::Var::PassRef(),
      char_set_interface_->GetDefaultCharSet(instance_->pp_instance()));
  ASSERT_TRUE(result.is_string());
  ASSERT_FALSE(result.AsString().empty());

  PASS();
}

std::vector<uint16_t> TestCharSet::UTF8ToUTF16(const std::string& utf8) {
  uint32_t result_len = 0;
  uint16_t* result = char_set_interface_->CharSetToUTF16(
      instance_->pp_instance(), utf8.c_str(),
      static_cast<uint32_t>(utf8.size()),
      "utf-8", PP_CHARSET_CONVERSIONERROR_FAIL, &result_len);

  std::vector<uint16_t> result_vector;
  if (!result)
    return result_vector;

  result_vector.assign(result, &result[result_len]);
  pp::Memory_Dev memory;
  memory.MemFree(result);
  return result_vector;
}
