// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_URL_LOADER_H_
#define PAPPI_TESTS_TEST_URL_LOADER_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_FileIOTrusted;

namespace pp {
class FileIO;
class FileRef;
class FileSystem;
class URLLoader;
class URLRequestInfo;
}

class TestURLLoader : public TestCase {
 public:
  explicit TestURLLoader(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string ReadEntireFile(pp::FileIO* file_io, std::string* data);
  std::string ReadEntireResponseBody(pp::URLLoader* loader,
                                     std::string* body);
  std::string LoadAndCompareBody(const pp::URLRequestInfo& request,
                                 const std::string& expected_body);
  int32_t OpenFileSystem(pp::FileSystem* file_system, std::string* message);
  int32_t PrepareFileForPost(const pp::FileRef& file_ref,
                             const std::string& data,
                             std::string* message);

  std::string TestBasicGET();
  std::string TestBasicPOST();
  std::string TestBasicFilePOST();
  std::string TestBasicFileRangePOST();
  std::string TestCompoundBodyPOST();
  std::string TestEmptyDataPOST();
  std::string TestBinaryDataPOST();
  std::string TestCustomRequestHeader();
  std::string TestFailsBogusContentLength();
  std::string TestStreamToFile();
  std::string TestSameOriginRestriction();
  std::string TestJavascriptURLRestriction();
  std::string TestCrossOriginRequest();
  std::string TestAuditURLRedirect();
  std::string TestAbortCalls();
  std::string TestUntendedLoad();

  const PPB_FileIOTrusted* file_io_trusted_interface_;
};

#endif  // PAPPI_TESTS_TEST_URL_LOADER_H_
