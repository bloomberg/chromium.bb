// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_url_loader.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(URLLoader);

namespace {

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO* file_io,
                          int32_t offset,
                          const std::string& data) {
  TestCompletionCallback callback(instance);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = data.size();

  while (write_offset < offset + size) {
    int32_t rv = file_io->Write(write_offset, &buf[write_offset - offset],
                                size - write_offset + offset, callback);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return rv;
    if (rv == 0)
      return PP_ERROR_FAILED;
    write_offset += rv;
  }

  return PP_OK;
}

}  // namespace

TestURLLoader::TestURLLoader(TestingInstance* instance)
    : TestCase(instance),
      file_io_trusted_interface_(NULL) {
}

bool TestURLLoader::Init() {
  file_io_trusted_interface_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  if (!file_io_trusted_interface_) {
    instance_->AppendError("FileIOTrusted interface not available");
  }
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestURLLoader::RunTest() {
  RUN_TEST_FORCEASYNC_AND_NOT(BasicGET);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicPOST);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicFilePOST);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicFileRangePOST);
  RUN_TEST_FORCEASYNC_AND_NOT(CompoundBodyPOST);
  RUN_TEST_FORCEASYNC_AND_NOT(EmptyDataPOST);
  RUN_TEST_FORCEASYNC_AND_NOT(BinaryDataPOST);
  RUN_TEST_FORCEASYNC_AND_NOT(CustomRequestHeader);
  RUN_TEST_FORCEASYNC_AND_NOT(FailsBogusContentLength);
  RUN_TEST_FORCEASYNC_AND_NOT(SameOriginRestriction);
  RUN_TEST_FORCEASYNC_AND_NOT(JavascriptURLRestriction);
  RUN_TEST_FORCEASYNC_AND_NOT(CrossOriginRequest);
  RUN_TEST_FORCEASYNC_AND_NOT(StreamToFile);
  RUN_TEST_FORCEASYNC_AND_NOT(AuditURLRedirect);
  RUN_TEST_FORCEASYNC_AND_NOT(AbortCalls);
  RUN_TEST_FORCEASYNC_AND_NOT(UntendedLoad);
}

std::string TestURLLoader::ReadEntireFile(pp::FileIO* file_io,
                                          std::string* data) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  char buf[256];
  int64_t offset = 0;

  for (;;) {
    int32_t rv = file_io->Read(offset, buf, sizeof(buf), callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Read force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
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
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  char buf[2];  // Small so that multiple reads are needed.

  for (;;) {
    int32_t rv = loader->ReadResponseBody(buf, sizeof(buf), callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("URLLoader::ReadResponseBody force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
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
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
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

int32_t TestURLLoader::OpenFileSystem(pp::FileSystem* file_system,
                                      std::string* message) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = file_system->Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING) {
    message->assign("FileSystem::Open force_async");
    return rv;
  }
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK) {
    message->assign("FileSystem::Open");
    return rv;
  }
  return rv;
}

int32_t TestURLLoader::PrepareFileForPost(
      const pp::FileRef& file_ref,
      const std::string& data,
      std::string* message) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileIO file_io(instance_);
  int32_t rv = file_io.Open(file_ref,
                            PP_FILEOPENFLAG_CREATE |
                            PP_FILEOPENFLAG_TRUNCATE |
                            PP_FILEOPENFLAG_WRITE,
                            callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING) {
    message->assign("FileIO::Open force_async");
    return rv;
  }
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK) {
    message->assign("FileIO::Open");
    return rv;
  }

  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, data);
  if (rv != PP_OK) {
    message->assign("FileIO::Write");
    return rv;
  }

  return rv;
}

std::string TestURLLoader::TestBasicGET() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  return LoadAndCompareBody(request, "hello\n");
}

std::string TestURLLoader::TestBasicPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestBasicFilePOST() {
  std::string message;

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = OpenFileSystem(&file_system, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::FileRef file_ref(file_system, "/file_post_test");
  std::string postdata("postdata");
  rv = PrepareFileForPost(file_ref, postdata, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendFileToBody(file_ref, 0);
  return LoadAndCompareBody(request, postdata);
}

std::string TestURLLoader::TestBasicFileRangePOST() {
  std::string message;

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = OpenFileSystem(&file_system, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::FileRef file_ref(file_system, "/file_range_post_test");
  std::string postdata("postdatapostdata");
  rv = PrepareFileForPost(file_ref, postdata, &message);
  if (rv != PP_OK)
    return ReportError(message.c_str(), rv);

  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendFileRangeToBody(file_ref, 4, 12, 0);
  return LoadAndCompareBody(request, postdata.substr(4, 12));
}

std::string TestURLLoader::TestCompoundBodyPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  std::string postdata1("post");
  request.AppendDataToBody(postdata1.data(), postdata1.length());
  std::string postdata2("data");
  request.AppendDataToBody(postdata2.data(), postdata2.length());
  return LoadAndCompareBody(request, postdata1 + postdata2);
}

std::string TestURLLoader::TestEmptyDataPOST() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.AppendDataToBody("", 0);
  return LoadAndCompareBody(request, "");
}

std::string TestURLLoader::TestBinaryDataPOST() {
  pp::URLRequestInfo request(instance_);
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
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echoheader?Foo");
  request.SetHeaders("Foo: 1");
  return LoadAndCompareBody(request, "1");
}

std::string TestURLLoader::TestFailsBogusContentLength() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod("POST");
  request.SetHeaders("Content-Length: 400");
  std::string postdata("postdata");
  request.AppendDataToBody(postdata.data(), postdata.length());

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();

  // The bad header should have made the request fail.
  ASSERT_TRUE(rv == PP_ERROR_FAILED);
  PASS();
}

std::string TestURLLoader::TestStreamToFile() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef body(response_info.GetBodyAsFileRef());
  if (body.is_null())
    return "URLResponseInfo::GetBody returned null";

  rv = loader.FinishStreamingToFile(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::FinishStreamingToFile force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::FinishStreamingToFile", rv);

  pp::FileIO reader(instance_);
  rv = reader.Open(body, PP_FILEOPENFLAG_READ, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
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
  pp::URLRequestInfo request(instance_);
  request.SetURL("http://www.google.com/");

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
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

std::string TestURLLoader::TestCrossOriginRequest() {
  // Get the document URL and use it to construct a URL that will be
  // considered cross-origin by the WebKit access control code, and yet be
  // reachable by the test server.
  PP_URLComponents_Dev components;
  pp::Var pp_document_url = pp::URLUtil_Dev::Get()->GetDocumentURL(
      *instance_, &components);
  std::string document_url = pp_document_url.AsString();
  // Replace "127.0.0.1" with "localhost".
  if (document_url.find("127.0.0.1") == std::string::npos)
    return "Can't construct a cross-origin URL";
  std::string cross_origin_url = document_url.replace(
      components.host.begin, components.host.len, "localhost");

  pp::URLRequestInfo request(instance_);
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();

  // We expect success since we allowed a cross-origin request.
  if (rv != PP_OK)
    return ReportError("URLLoader::Open()", rv);

  PASS();
}

std::string TestURLLoader::TestJavascriptURLRestriction() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("javascript:foo = bar");

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();

  // We expect a failure.
  if (rv != PP_ERROR_NOACCESS) {
    if (rv == PP_OK) {
      return "URLLoader::Open() failed to block a Javascript request.";
    } else {
      return ReportError("URLLoader::Open()", rv);
    }
  }

  PASS();
}

// This test should cause a redirect and ensure that the loader runs
// the callback, rather than following the redirect.
std::string TestURLLoader::TestAuditURLRedirect() {
  pp::URLRequestInfo request(instance_);
  // This path will cause the server to return a 301 redirect.
  request.SetURL("/server-redirect?www.google.com");
  request.SetFollowRedirects(false);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
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
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv;

  // Abort |Open()|.
  {
    callback.reset_run_count();
    rv = pp::URLLoader(*instance_).Open(request, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("URLLoader::Open force_async", rv);
    if (callback.run_count() > 0)
      return "URLLoader::Open ran callback synchronously.";
    if (rv == PP_OK_COMPLETIONPENDING) {
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
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("URLLoader::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("URLLoader::Open", rv);

      callback.reset_run_count();
      rv = loader.ReadResponseBody(buf, sizeof(buf), callback);
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("URLLoader::ReadResponseBody force_async", rv);
    }  // Destroy |loader|.
    if (rv == PP_OK_COMPLETIONPENDING) {
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

std::string TestURLLoader::TestUntendedLoad() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetRecordDownloadProgress(true);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(*instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("URLLoader::Open", rv);

  // We received the response callback. Yield until the network code has called
  // the loader's didReceiveData and didFinishLoading methods before we give it
  // another callback function, to make sure the loader works with no callback.
  int64_t bytes_received = 0;
  int64_t total_bytes_to_be_received = 0;
  while (true) {
    loader.GetDownloadProgress(&bytes_received, &total_bytes_to_be_received);
    if (total_bytes_to_be_received <= 0)
      return ReportError("URLLoader::GetDownloadProgress total size",
          total_bytes_to_be_received);
    if (bytes_received == total_bytes_to_be_received)
      break;
    pp::Module::Get()->core()->CallOnMainThread(10, callback);
    callback.WaitForResult();
  }

  // The loader should now have the data and have finished successfully.
  std::string body;
  std::string error = ReadEntireResponseBody(&loader, &body);
  if (!error.empty())
    return error;
  if (body != "hello\n")
    return ReportError("Couldn't read data", rv);

  PASS();
}

// TODO(viettrungluu): Add tests for FollowRedirect,
// Get{Upload,Download}Progress, Close (including abort tests if applicable).
// TODO(darin): Add a test for GrantUniversalAccess.
