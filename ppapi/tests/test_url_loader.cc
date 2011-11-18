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
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
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
      file_io_trusted_interface_(NULL),
      url_loader_trusted_interface_(NULL) {
}

bool TestURLLoader::Init() {
  file_io_trusted_interface_ = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  if (!file_io_trusted_interface_) {
    instance_->AppendError("FileIOTrusted interface not available");
  }
  url_loader_trusted_interface_ = static_cast<const PPB_URLLoaderTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
  if (!url_loader_trusted_interface_) {
    instance_->AppendError("URLLoaderTrusted interface not available");
  }
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestURLLoader::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(BasicGET, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicPOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicFilePOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(BasicFileRangePOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(CompoundBodyPOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(EmptyDataPOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(BinaryDataPOST, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(CustomRequestHeader, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(FailsBogusContentLength, filter);
  // Disable portion of test which fails when the HTTP server's
  // data_dir is moved to PRODUCT_DIR.
  // http://code.google.com/p/chromium/issues/detail?id=103690
  //  RUN_TEST_FORCEASYNC_AND_NOT(SameOriginRestriction, filter);
  //  RUN_TEST_FORCEASYNC_AND_NOT(CrossOriginRequest, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(JavascriptURLRestriction, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(MethodRestriction, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(HeaderRestriction, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(CustomReferrer, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(CustomContentTransferEncoding, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(StreamToFile, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(AuditURLRedirect, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(AbortCalls, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(UntendedLoad, filter);
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

std::string TestURLLoader::GetReachableCrossOriginURL() {
  // Get the document URL and use it to construct a URL that will be
  // considered cross-origin by the WebKit access control code, and yet be
  // reachable by the test server.
  PP_URLComponents_Dev components;
  pp::Var pp_document_url = pp::URLUtil_Dev::Get()->GetDocumentURL(
      *instance_, &components);
  std::string document_url = pp_document_url.AsString();
  // Replace "127.0.0.1" with "localhost".
  ASSERT_TRUE(document_url.find("127.0.0.1") != std::string::npos);
  return document_url.replace(
      components.host.begin, components.host.len, "localhost");
}

int32_t TestURLLoader::OpenUntrusted(const std::string& method,
                                     const std::string& header) {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod(method);
  request.SetHeaders(header);

  return OpenUntrusted(request);
}

int32_t TestURLLoader::OpenTrusted(const std::string& method,
                                   const std::string& header) {
  pp::URLRequestInfo request(instance_);
  request.SetURL("/echo");
  request.SetMethod(method);
  request.SetHeaders(header);

  return OpenTrusted(request);
}

int32_t TestURLLoader::OpenUntrusted(const pp::URLRequestInfo& request) {
  return Open(request, false);
}

int32_t TestURLLoader::OpenTrusted(const pp::URLRequestInfo& request) {
  return Open(request, true);
}

int32_t TestURLLoader::Open(const pp::URLRequestInfo& request,
                            bool trusted) {
  pp::URLLoader loader(*instance_);
  if (trusted)
    url_loader_trusted_interface_->GrantUniversalAccess(loader.pp_resource());
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = loader.Open(request, callback);

  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  else if (force_async_)
    ReportError("URLLoader::Open force_async", rv);

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

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted request with bogus Content-Length restriction", rv);

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

// If a cross-origin request is not specified, the load should fail only for
// untrusted loaders.
std::string TestURLLoader::TestSameOriginRestriction() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL();
  request.SetURL(cross_origin_url);

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted, unintended cross-origin request restriction", rv);
  rv = OpenTrusted(request);
  if (rv != PP_OK)
    return ReportError("Trusted cross-origin request", rv);

  PASS();
}

// If a cross-origin request is specified, and the URL is reachable, the load
// should succeed.
std::string TestURLLoader::TestCrossOriginRequest() {
  pp::URLRequestInfo request(instance_);
  std::string cross_origin_url = GetReachableCrossOriginURL();
  request.SetURL(cross_origin_url);
  request.SetAllowCrossOriginRequests(true);

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_OK)
    return ReportError(
        "Untrusted, intended cross-origin request", rv);
  rv = OpenTrusted(request);
  if (rv != PP_OK)
    return ReportError("Trusted cross-origin request", rv);

  PASS();
}

// Javascript URLs are only reachable by trusted loaders.
std::string TestURLLoader::TestJavascriptURLRestriction() {
  pp::URLRequestInfo request(instance_);
  request.SetURL("javascript:foo = bar");

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted Javascript URL request restriction", rv);
  // TODO(bbudge) Fix Javascript URLs for trusted loaders.
  // http://code.google.com/p/chromium/issues/detail?id=103062
  // rv = OpenTrusted(request);
  // if (rv == PP_ERROR_NOACCESS)
  //  return ReportError(
  //      "Trusted Javascript URL request", rv);

  PASS();
}

// HTTP methods are restricted only for untrusted loaders. Forbidden
// methods are CONNECT, TRACE, and TRACK, and any string that is not a valid
// token (containing special characters like CR, LF).
// http://www.w3.org/TR/XMLHttpRequest/
std::string TestURLLoader::TestMethodRestriction() {
  ASSERT_EQ(OpenUntrusted("cOnNeCt", ""), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("tRaCk", ""), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("tRaCe", ""), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("POST\x0d\x0ax-csrf-token:\x20test1234", ""),
                          PP_ERROR_NOACCESS);

  ASSERT_EQ(OpenTrusted("cOnNeCt", ""), PP_OK);
  ASSERT_EQ(OpenTrusted("tRaCk", ""), PP_OK);
  ASSERT_EQ(OpenTrusted("tRaCe", ""), PP_OK);

  PASS();
}

// HTTP methods are restricted only for untrusted loaders. Try all headers
// that are forbidden by http://www.w3.org/TR/XMLHttpRequest/.
std::string TestURLLoader::TestHeaderRestriction() {
  ASSERT_EQ(OpenUntrusted("GET", "Accept-Charset:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Accept-Encoding:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Connection:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Content-Length:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Cookie:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Cookie2:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted(
      "GET", "Content-Transfer-Encoding:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Date:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Expect:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Host:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Keep-Alive:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Referer:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "TE:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Trailer:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Transfer-Encoding:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Upgrade:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "User-Agent:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Via:\n"), PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted(
      "GET", "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==:\n"),
          PP_ERROR_NOACCESS);
  ASSERT_EQ(OpenUntrusted("GET", "Sec-foo:\n"), PP_ERROR_NOACCESS);

  ASSERT_EQ(OpenTrusted("GET", "Accept-Charset:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Accept-Encoding:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Connection:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Content-Length:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Cookie:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Cookie2:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted(
      "GET", "Content-Transfer-Encoding:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Date:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Expect:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Host:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Keep-Alive:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Referer:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "TE:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Trailer:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Transfer-Encoding:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Upgrade:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "User-Agent:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Via:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted(
      "GET", "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==:\n"), PP_OK);
  ASSERT_EQ(OpenTrusted("GET", "Sec-foo:\n"), PP_OK);

  PASS();
}

// Custom referrers are only allowed for trusted loaders.
std::string TestURLLoader::TestCustomReferrer() {
  pp::URLRequestInfo request(instance_);
  request.SetCustomReferrerURL("http://www.google.com/");

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted request with custom referrer restriction", rv);
  rv = OpenTrusted(request);
  if (rv != PP_OK)
    return ReportError("Trusted request with custom referrer", rv);

  PASS();
}

// Custom transfer encodings are only allowed for trusted loaders.
std::string TestURLLoader::TestCustomContentTransferEncoding() {
  pp::URLRequestInfo request(instance_);
  request.SetCustomContentTransferEncoding("foo");

  int32_t rv;
  rv = OpenUntrusted(request);
  if (rv != PP_ERROR_NOACCESS)
    return ReportError(
        "Untrusted request with content-transfer-encoding restriction", rv);
  rv = OpenTrusted(request);
  if (rv != PP_OK)
    return ReportError("Trusted request with content-transfer-encoding", rv);

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
