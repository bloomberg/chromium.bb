// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_url_loader.h"

#include <stdio.h>
#include <string>

#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_url_loader_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/dev/url_loader_dev.h"
#include "ppapi/cpp/dev/url_request_info_dev.h"
#include "ppapi/cpp/dev/url_response_info_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(URLLoader);

bool TestURLLoader::Init() {
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestURLLoader::RunTest() {
  RUN_TEST(BasicGET);
  RUN_TEST(BasicPOST);
  RUN_TEST(CompoundBodyPOST);
  RUN_TEST(EmptyDataPOST);
  RUN_TEST(BinaryDataPOST);
  RUN_TEST(CustomRequestHeader);
  RUN_TEST(IgnoresBogusContentLength);
  RUN_TEST(SameOriginRestriction);
  RUN_TEST(StreamToFile);
}

std::string TestURLLoader::ReadEntireFile(pp::FileIO_Dev* file_io,
                                          std::string* data) {
  TestCompletionCallback callback;
  char buf[256];
  int64_t offset = 0;

  for (;;) {
    int32_t rv = file_io->Read(offset, buf, sizeof(buf), callback);
    if (rv == PP_ERROR_WOULDBLOCK)
      rv = callback.WaitForResult();
    if (rv < 0)
      return ReportError("FileIO::Read", rv);
    if (rv == 0)
      break;
    offset += rv;
    data->append(buf, rv);
  }

  return "";
}

std::string TestURLLoader::ReadEntireResponseBody(pp::URLLoader_Dev* loader,
                                                  std::string* body) {
  TestCompletionCallback callback;
  char buf[256];

  for (;;) {
    int32_t rv = loader->ReadResponseBody(buf, sizeof(buf), callback);
    if (rv == PP_ERROR_WOULDBLOCK)
      rv = callback.WaitForResult();
    if (rv < 0)
      return ReportError("URLLoader::ReadResponseBody", rv);
    if (rv == 0)
      break;
    body->append(buf, rv);
  }

  return "";
}

std::string TestURLLoader::LoadAndCompareBody(
    const pp::URLRequestInfo_Dev& request,
    const std::string& expected_body) {
  TestCompletionCallback callback;

  pp::URLLoader_Dev loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  pp::URLResponseInfo_Dev response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  std::string body;
  std::string error = ReadEntireResponseBody(&loader, &body);
  if (!error.empty())
    return error;

  if (body.size() != expected_body.size())
    return "URLLoader::ReadResponseBody returned unexpected content length";
  if (body != expected_body)
    return "URLLoader::ReadResponseBody returned unexpected content";

  return "";
}

std::string TestURLLoader::TestBasicGET() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("test_url_loader_data/hello.txt");
  return LoadAndCompareBody(request, "hello\n");
}

std::string TestURLLoader::TestBasicPOST() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestCompoundBodyPOST() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata1("post");
  request.AppendDataToBody(postdata1.data(), postdata1.length());
  std::string postdata2("data");
  request.AppendDataToBody(postdata2.data(), postdata2.length());
  return LoadAndCompareBody(request, postdata1 + postdata2);
}

std::string TestURLLoader::TestEmptyDataPOST() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendDataToBody("", 0);
  return LoadAndCompareBody(request, "");
}

std::string TestURLLoader::TestBinaryDataPOST() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  const char postdata_chars[] =
      "\x00\x01\x02\x03\x04\x05postdata\xfa\xfb\xfc\xfd\xfe\xff";
  std::string postdata(postdata_chars,
                       sizeof(postdata_chars) / sizeof(postdata_chars[0]));
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestCustomRequestHeader() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echoheader?Foo");
  request.SetHeaders("Foo: 1");
  return LoadAndCompareBody(request, "1");
}

std::string TestURLLoader::TestIgnoresBogusContentLength() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.SetHeaders("Content-Length: 400");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestStreamToFile() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback;

  pp::URLLoader_Dev loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  pp::URLResponseInfo_Dev response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef_Dev body(response_info.GetBody());
  if (body.is_null())
    return "URLResponseInfo::GetBody returned null";

  rv = loader.FinishStreamingToFile(callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::FinishStreamingToFile", rv);


  pp::FileIO_Dev reader;
  rv = reader.Open(body, PP_FILEOPENFLAG_READ, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  std::string data;
  std::string error = ReadEntireFile(&reader, &data);
  if (!error.empty())
    return error;

  std::string expected_body = "hello\n";
  if (data.size() != expected_body.size())
    return "ReadEntireFile returned unexpected content length";
  if (data != expected_body)
    return "ReadEntireFile returned unexpected content";

  int32_t file_descriptor = reader.GetOSFileDescriptor();
  if (file_descriptor < 0)
    return "FileIO::GetOSFileDescriptor() returned a bad file descriptor.";

  return "";
}

std::string TestURLLoader::TestSameOriginRestriction() {
  pp::URLRequestInfo_Dev request;
  request.SetURL("http://www.google.com/");

  TestCompletionCallback callback;

  pp::URLLoader_Dev loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();

  // We expect a failure.
  if (rv != PP_ERROR_NOACCESS) {
    if (rv == PP_OK) {
      return "URLLoader::Open() failed to block a cross-origin request.";
    } else {
      return ReportError("URLLoader::Open()", rv);
    }
  }

  return "";
}

// TODO(darin): Add a test for GrantUniversalAccess.
