// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FLASH_CLIPBOARD_H_
#define PPAPI_TESTS_TEST_FLASH_CLIPBOARD_H_

#include <string>

#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

class TestFlashClipboard : public TestCase {
 public:
  explicit TestFlashClipboard(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  // Helpers.
  PP_Bool IsFormatAvailable(PP_Flash_Clipboard_Format format);
  std::string ReadStringVar(PP_Flash_Clipboard_Format format);
  int32_t WriteStringVar(PP_Flash_Clipboard_Format format,
                         const std::string& input);
  bool ReadAndMatchPlainText(const std::string& input);
  bool ReadAndMatchHTML(const std::string& input);

  // Tests.
  std::string TestReadWritePlainText();
  std::string TestReadWriteHTML();
  std::string TestReadWriteMultipleFormats();
  std::string TestClear();

  const PPB_Flash_Clipboard* clipboard_interface_;
};

#endif  // PAPPI_TESTS_TEST_FLASH_FULLSCREEN_H_
