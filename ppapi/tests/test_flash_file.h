// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FLASH_FILE_H_
#define PPAPI_TESTS_TEST_FLASH_FILE_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestFlashFile: public TestCase {
 public:
  explicit TestFlashFile(TestingInstance* instance);
  virtual ~TestFlashFile();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  // TODO(yzshen): Add more test cases for PPB_Flash_File_ModuleLocal and
  // PPB_Flash_File_FileRef.
  std::string TestCreateTemporaryFile();

  // Gets the number of files and directories under the module-local root
  // directory.
  std::string GetItemCountUnderModuleLocalRoot(size_t* item_count);
};

#endif  // PPAPI_TESTS_TEST_FLASH_FILE_H_
