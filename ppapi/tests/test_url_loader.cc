// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_url_loader.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(URLLoader);

TestURLLoader::TestURLLoader(TestingInstance* instance)
    : TestCase(instance),
      file_io_trusted_interface_(NULL) {
}

bool TestURLLoader::Init() {
  file_io_trusted_interface_ = static_cast<const PPB_FileIOTrusted_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_DEV_INTERFACE));
  if (!file_io_trusted_interface_) {
    instance_->AppendError("FileIOTrusted interface not available");
  }
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
  RUN_TEST(AuditURLRedirect);
  RUN_TEST(AbortCalls);
}

std::string TestURLLoader::ReadEntireFile(pp::FileIO_Dev* file_io,
                                          std::string* data) {
  TestCompletionCallback callback(instance_->pp_instance());
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

  PASS();
}

std::string TestURLLoader::ReadEntireResponseBody(pp::URLLoader* loader,
                                                  std::string* body) {
  TestCompletionCallback callback(instance_->pp_instance());
  char buf[2];  // Small so that multiple reads are needed.

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

  PASS();
}

std::string TestURLLoader::LoadAndCompareBody(
    const pp::URLRequestInfo& request,
    const std::string& expected_body) {
  TestCompletionCallback callback(instance_->pp_instance());

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
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

  PASS();
}

std::string TestURLLoader::TestBasicGET() {
  pp::URLRequestInfo request;
  request.SetURL("test_url_loader_data/hello.txt");
  return LoadAndCompareBody(request, "hello\n");
}

std::string TestURLLoader::TestBasicPOST() {
  pp::URLRequestInfo request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestCompoundBodyPOST() {
  pp::URLRequestInfo request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata1("post");
  request.AppendDataToBody(postdata1.data(), postdata1.length());
  std::string postdata2("data");
  request.AppendDataToBody(postdata2.data(), postdata2.length());
  return LoadAndCompareBody(request, postdata1 + postdata2);
}

std::string TestURLLoader::TestEmptyDataPOST() {
  pp::URLRequestInfo request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendDataToBody("", 0);
  return LoadAndCompareBody(request, "");
}

std::string TestURLLoader::TestBinaryDataPOST() {
  pp::URLRequestInfo request;
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
  pp::URLRequestInfo request;
  request.SetURL("/echoheader?Foo");
  request.SetHeaders("Foo: 1");
  return LoadAndCompareBody(request, "1");
}

std::string TestURLLoader::TestIgnoresBogusContentLength() {
  pp::URLRequestInfo request;
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.SetHeaders("Content-Length: 400");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestStreamToFile() {
  pp::URLRequestInfo request;
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance());

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef_Dev body(response_info.GetBodyAsFileRef());
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

  int32_t file_descriptor = file_io_trusted_interface_->GetOSFileDescriptor(
      reader.pp_resource());
  if (file_descriptor < 0)
    return "FileIO::GetOSFileDescriptor() returned a bad file descriptor.";

  PASS();
}

std::string TestURLLoader::TestSameOriginRestriction() {
  pp::URLRequestInfo request;
  request.SetURL("http://www.google.com/");

  TestCompletionCallback callback(instance_->pp_instance());

  pp::URLLoader loader(*instance_);
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

  PASS();
}

// This test should cause a redirect and ensure that the loader runs
// the callback, rather than following the redirect.
std::string TestURLLoader::TestAuditURLRedirect() {
  pp::URLRequestInfo request;
  // This path will cause the server to return a 301 redirect.
  request.SetURL("/server-redirect?www.google.com");
  request.SetFollowRedirects(false);

  TestCompletionCallback callback(instance_->pp_instance());

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  // Checks that the response indicates a redirect, and that the URL
  // is correct.
  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 301)
    return "Response status should be 301";
  if (response_info.GetRedirectURL().AsString() != "www.google.com")
    return "Redirect URL should be www.google.com";

  PASS();
}

std::string TestURLLoader::TestAbortCalls() {
  pp::URLRequestInfo request;
  request.SetURL("test_url_loader_data/hello.txt");

  TestCompletionCallback callback(instance_->pp_instance());
  int32_t rv;

  // Abort |Open()|.
  {
    callback.reset_run_count();
    rv = pp::URLLoader(*instance_).Open(request, callback);
    if (callback.run_count() > 0)
      return "URLLoader::Open ran callback synchronously.";
    if (rv == PP_ERROR_WOULDBLOCK) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "URLLoader::Open not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("URLLoader::Open", rv);
    }
  }

  // Abort |ReadResponseBody()|.
  {
    char buf[2] = { 0 };
    {
      pp::URLLoader loader(*instance_);
      rv = loader.Open(request, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("URLLoader::Open", rv);

      callback.reset_run_count();
      rv = loader.ReadResponseBody(buf, sizeof(buf), callback);
    }  // Destroy |loader|.
    if (rv == PP_ERROR_WOULDBLOCK) {
      // Save a copy and make sure |buf| doesn't get written to.
      char buf_copy[2];
      memcpy(&buf_copy, &buf, sizeof(buf));
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "URLLoader::ReadResponseBody not aborted.";
      if (memcmp(&buf_copy, &buf, sizeof(buf)) != 0)
        return "URLLoader::ReadResponseBody wrote data after resource "
               "destruction.";
    } else if (rv != PP_OK) {
      return ReportError("URLLoader::ReadResponseBody", rv);
    }
  }

  // TODO(viettrungluu): More abort tests (but add basic tests first).
  // Also test that Close() aborts properly. crbug.com/69457

  PASS();
}

// TODO(viettrungluu): Add tests for FollowRedirect,
// Get{Upload,Download}Progress, Close (including abort tests if applicable).
// TODO(darin): Add a test for GrantUniversalAccess.
