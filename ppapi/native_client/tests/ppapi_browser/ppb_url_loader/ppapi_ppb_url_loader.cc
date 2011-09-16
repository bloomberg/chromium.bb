// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/nacl_scoped_ptr.h"

#include "native_client/src/shared/platform/nacl_check.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"

#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "native_client/tests/ppapi_test_lib/testable_callback.h"

namespace {

// Same content as file kExistingURL in this directory.
const char* kContentsOfExistingURL = "Lorem ipsum dolor sit.\n";
const char* kNonExistingURL = "does_not_exist";
const char* kExistingURL = "ppapi_ppb_url_loader.txt";
const char* kRedirectURL = "redirect-to-existing.txt";
const char* kNoHeaders = NULL;

const PP_Bool kStreamToFile = PP_TRUE;
const PP_Bool kDontStreamToFile = PP_FALSE;

const PP_Bool kDownloadProgress = PP_TRUE;
const PP_Bool kNoDownloadProgress = PP_FALSE;

const PP_Bool kFollowRedirects = PP_TRUE;
const PP_Bool kDontFollowRedirects = PP_FALSE;

// Helper functions, these are candidates for being moved to a helper_lib
void CheckResponse(PP_Resource response,
                   int http_status,
                   const char* status_line,
                   const char* response_url,
                   const char* headers) {
  PP_Var var;
  var = PPBURLResponseInfo()->GetProperty(
      response, PP_URLRESPONSEPROPERTY_STATUSCODE);
  EXPECT_VAR_INT(var, http_status);

  if (status_line != NULL) {
    var = PPBURLResponseInfo()->GetProperty(
        response, PP_URLRESPONSEPROPERTY_STATUSLINE);
    EXPECT_VAR_STRING(var, status_line);
    PPBVar()->Release(var);
  }

  if (response_url != NULL) {
    // TODO(robertm): This needs a regexp pass to get rid of the port number
    //                which is non-deterministic.
    var = PPBURLResponseInfo()->GetProperty(
        response, PP_URLRESPONSEPROPERTY_URL);
    EXPECT_VAR_STRING(var, response_url);
    PPBVar()->Release(var);
  }

  if (headers != NULL) {
    // TODO(robertm): This needs a regexp pass to get rid of the date, etc
    //                 which are non-deterministic.
    var = PPBURLResponseInfo()->GetProperty(
        response, PP_URLRESPONSEPROPERTY_HEADERS);
    EXPECT_VAR_STRING(var, headers);
    PPBVar()->Release(var);
  }
}


PP_Resource MakeRequest(const char* url,
                        const char* method,
                        const char* headers,
                        PP_Bool to_file,
                        PP_Bool download_progress,
                        PP_Bool follow_redirects) {
  PP_Resource request = PPBURLRequestInfo()->Create(pp_instance());
  EXPECT(request != kInvalidResource);
  PP_Var var;

  if (url != NULL) {
    var = PP_MakeString(url);
    EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
        request, PP_URLREQUESTPROPERTY_URL, var));
    PPBVar()->Release(var);
  }

  if (method != NULL) {
    var = PP_MakeString(method);
    EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
        request, PP_URLREQUESTPROPERTY_METHOD, var));
    PPBVar()->Release(var);
  }

  if (headers != NULL) {
    var = PP_MakeString(method);
    EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
        request, PP_URLREQUESTPROPERTY_HEADERS, var));
    PPBVar()->Release(var);
  }

  var = PP_MakeBool(to_file);
  EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
      request, PP_URLREQUESTPROPERTY_STREAMTOFILE, var));

  var = PP_MakeBool(download_progress);
  EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
      request, PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, var));

  var = PP_MakeBool(follow_redirects);
  EXPECT(PP_TRUE == PPBURLRequestInfo()->SetProperty(
      request, PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, var));
  return request;
}

PP_Resource MakeDummyRequest() {
  return MakeRequest(kNonExistingURL,
                     "GET",
                     kNoHeaders,
                     kDontStreamToFile,
                     kDownloadProgress,
                     kFollowRedirects);
}


int32_t ReadAndVerify(bool is_file_io,
                      PP_Resource resource,
                      int offset,
                      int buffer_size,
                      const nacl::string& expected) {
  nacl::scoped_array<char> buffer(new char[buffer_size]);
  TestableCallback callback(pp_instance(), true);
  nacl::string actual;
  int32_t rv;
  for (;;) {
    callback.Reset();
    if (is_file_io) {
      rv = PPBFileIO()->Read(resource,
                           offset + actual.size(),
                           buffer.get(),
                           buffer_size,
                           callback.GetCallback());
    } else {
      rv = PPBURLLoader()->ReadResponseBody(
        resource, buffer.get(), buffer_size, callback.GetCallback());
    }
    EXPECT(PP_OK_COMPLETIONPENDING == rv);
    rv = callback.WaitForResult();
    if (rv == 0) {
      break;
    } else if (rv < 0) {
      return rv;
    } else {
      actual.append(buffer.get(), rv);
    }
  }

  // rv was zero
  EXPECT(actual == expected);
  return 0;
}


int32_t ReadAndVerifyResponseBody(PP_Resource loader,
                                  int buffer_size,
                                  const nacl::string& expected) {
  return ReadAndVerify(false, loader, 0, buffer_size, expected);
}


int32_t ReadAndVerifyFileContents(PP_Resource file_io,
                                  int offset,
                                  int buffer_size,
                                  const nacl::string& expected) {
  return ReadAndVerify(true, file_io, offset, buffer_size, expected);
}


void TestOpenSuccess() {
  int32_t rv;
  TestableCallback callback(pp_instance(), true);
  PP_Resource loader;
  PP_Resource response;
  PP_Resource request = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                    kDontStreamToFile,
                                    kDownloadProgress,
                                    kFollowRedirects);

  LOG_TO_BROWSER("open and read entire existing file (all at once)");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);
  ReadAndVerifyResponseBody(loader, 1024, kContentsOfExistingURL);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open and read entire existing file (one byte at a time)");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);
  ReadAndVerifyResponseBody(loader, 1, kContentsOfExistingURL);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestOpenRedirect() {
  int32_t rv;
  TestableCallback callback(pp_instance(), true);
  PP_Resource loader;
  PP_Resource response;
  PP_Resource request_no_redirect = MakeRequest(kRedirectURL, "GET", kNoHeaders,
                                                kDontStreamToFile,
                                                kDownloadProgress,
                                                kDontFollowRedirects);
  PP_Resource request_redirect = MakeRequest(kRedirectURL, "GET", kNoHeaders,
                                             kDontStreamToFile,
                                             kDownloadProgress,
                                             kFollowRedirects);

  LOG_TO_BROWSER("redirect do NOT follow");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(
      loader, request_no_redirect, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 301, "Moved", NULL, NULL);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("redirect follow");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request_redirect, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);
  // TODO(robertm): also try smaller sizes, e.g. one and zero
  EXPECT(PP_OK ==
      ReadAndVerifyResponseBody(loader, 1024, kContentsOfExistingURL));

  PPBCore()->ReleaseResource(loader);
  PPBCore()->ReleaseResource(request_redirect);
  PPBCore()->ReleaseResource(request_no_redirect);

  TEST_PASSED;
}


void TestOpenFailure() {
  int32_t rv;
  TestableCallback callback(pp_instance(), true);
  PP_Resource request = MakeDummyRequest();
  PP_Resource loader = PPBURLLoader()->Create(pp_instance());

  LOG_TO_BROWSER("open with non existing url");
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  PP_Resource response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 404, "File not found", NULL, NULL);

  PPBCore()->ReleaseResource(request);
  PPBCore()->ReleaseResource(loader);

  TEST_PASSED;
}


void TestStreamToFile() {
  int32_t rv;
  TestableCallback callback(pp_instance(), true);
  PP_Resource loader;
  PP_Resource file_ref;
  PP_Resource response;
  PP_Resource file_io;
  PP_Resource request = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                    kStreamToFile,
                                    kDownloadProgress,
                                    kFollowRedirects);

  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);

  file_ref = PPBURLResponseInfo()->GetBodyAsFileRef(response);
  callback.Reset();
  rv = PPBURLLoader()->FinishStreamingToFile(loader, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  callback.Reset();
  file_io = PPBFileIO()->Create(pp_instance());
  rv = PPBFileIO()->Open(
      file_io, file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  // TODO(robertm): add tests with different buffer sizes
  ReadAndVerifyFileContents(file_io, 0, 1024, kContentsOfExistingURL);

  PPBCore()->ReleaseResource(file_io);
  PPBCore()->ReleaseResource(file_ref);
  PPBCore()->ReleaseResource(response);
  PPBCore()->ReleaseResource(loader);
  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestOpenSimple() {
  int32_t rv;
  TestableCallback callback(pp_instance(), true);
  PP_Resource request = MakeDummyRequest();
  PP_Resource loader;

  LOG_TO_BROWSER("open with loader/request invalid");
  callback.Reset();
  rv = PPBURLLoader()->Open(
      kInvalidResource, kInvalidResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);
  callback.Reset();
  rv = PPBURLLoader()->Open(
      kNotAResource, kNotAResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  LOG_TO_BROWSER("open with loader invalid");
  callback.Reset();
  rv = PPBURLLoader()->Open(kInvalidResource, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);
  callback.Reset();
  rv = PPBURLLoader()->Open(kNotAResource, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);
  callback.Reset();
  rv = PPBURLLoader()->Open(request, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  LOG_TO_BROWSER("open with request invalid");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, kInvalidResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADARGUMENT == rv);
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, kNotAResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADARGUMENT == rv);
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, loader, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADARGUMENT == rv);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open (synchronous) with blocking callback on main thread");
  loader = PPBURLLoader()->Create(pp_instance());
  // We are on the main thread, performing a sync call should fail
  rv = PPBURLLoader()->Open(loader, request, PP_BlockUntilComplete());
  EXPECT(PP_ERROR_BLOCKS_MAIN_THREAD == rv);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open (asynchronous) normal");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open with with released loader");
  loader = PPBURLLoader()->Create(pp_instance());
  PPBCore()->ReleaseResource(loader);
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestDoubleOpen() {
  int32_t rv;
  PP_Resource loader;
  TestableCallback callback(pp_instance(), true);
  PP_Resource request1 = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                     kDontStreamToFile,
                                     kDownloadProgress,
                                     kDontFollowRedirects);
  PP_Resource request2 = MakeRequest(kNonExistingURL, "GET", kNoHeaders,
                                     kDontStreamToFile,
                                     kDownloadProgress,
                                     kDontFollowRedirects);

  LOG_TO_BROWSER("open two requests on the same loader without Close");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request1, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request2, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_INPROGRESS == rv);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open two requests on the same loader with Close");
  // NOTE: using Close should NOT make a difference either
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request1, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);

  PPBURLLoader()->Close(loader);

  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request2, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_INPROGRESS == rv);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request1);
  PPBCore()->ReleaseResource(request2);

  TEST_PASSED;
}


void TestProgressSimple() {
  int32_t rv;
  int64_t received;
  int64_t total;
  TestableCallback callback(pp_instance(), true);
  PP_Resource loader;
  PP_Resource req_bad = MakeDummyRequest();
  PP_Resource req_good = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                     kDontStreamToFile,
                                     kDownloadProgress,
                                     kDontFollowRedirects);
  PP_Resource req_good_no_progress = MakeRequest(kExistingURL,
                                                 "GET",
                                                 kNoHeaders,
                                                 kDontStreamToFile,
                                                 kNoDownloadProgress,
                                                 kDontFollowRedirects);

  LOG_TO_BROWSER("progress on invalid loader");
  rv = PPBURLLoader()->GetDownloadProgress(kInvalidResource, &received, &total);
  EXPECT(PP_FALSE == rv);
  rv = PPBURLLoader()->GetDownloadProgress(kNotAResource, &received, &total);
  EXPECT(PP_FALSE == rv);
  rv = PPBURLLoader()->GetDownloadProgress(req_bad, &received, &total);
  EXPECT(PP_FALSE == rv);

  LOG_TO_BROWSER("progress for failed request");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, req_bad, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  rv = PPBURLLoader()->GetDownloadProgress(loader, &received, &total);
  EXPECT(PP_TRUE == rv);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("progress for request");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, req_good, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  rv = PPBURLLoader()->GetDownloadProgress(loader, &received, &total);
  EXPECT(PP_TRUE == rv);
  const int num_bytes_expected = strlen(kContentsOfExistingURL);
  EXPECT(total == num_bytes_expected);
  ReadAndVerifyResponseBody(loader, 1024, kContentsOfExistingURL);
  rv = PPBURLLoader()->GetDownloadProgress(loader, &received, &total);
  EXPECT(PP_TRUE == rv);
  EXPECT(total == num_bytes_expected);
  EXPECT(received == num_bytes_expected);

  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("progress for request 'without progress'");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(
      loader, req_good_no_progress, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  rv = PPBURLLoader()->GetDownloadProgress(loader, &received, &total);
  EXPECT(PP_FALSE == rv);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(req_bad);
  PPBCore()->ReleaseResource(req_good);
  PPBCore()->ReleaseResource(req_good_no_progress);

  TEST_PASSED;
}


void TestResponseInfoSimple() {
  PP_Resource response;
  PP_Resource request = MakeDummyRequest();

  LOG_TO_BROWSER("response info on invalid loader");
  response = PPBURLLoader()->GetResponseInfo(kInvalidResource);
  EXPECT(kInvalidResource == response);
  response = PPBURLLoader()->GetResponseInfo(kNotAResource);
  EXPECT(kInvalidResource == response);
  response = PPBURLLoader()->GetResponseInfo(request);
  EXPECT(kInvalidResource == response);

  LOG_TO_BROWSER("response info without open");
  PP_Resource loader = PPBURLLoader()->Create(pp_instance());
  response = PPBURLLoader()->GetResponseInfo(loader);
  EXPECT(kInvalidResource == response);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestCloseSimple() {
  PP_Resource request = MakeDummyRequest();
  LOG_TO_BROWSER("close on invalid loader");
  // we just make sure we do not crash here.
  PPBURLLoader()->Close(kInvalidResource);
  PPBURLLoader()->Close(kNotAResource);
  PPBURLLoader()->Close(request);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestReadSimple() {
  int32_t rv;
  char buffer[1024];
  PP_Resource loader;
  TestableCallback callback(pp_instance(), true);

  PP_Resource request = MakeDummyRequest();

  LOG_TO_BROWSER("read on invalid loader");
  callback.Reset();
  rv = PPBURLLoader()->ReadResponseBody(
      kInvalidResource, buffer, sizeof buffer, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  callback.Reset();
  rv = PPBURLLoader()->ReadResponseBody(
      kNotAResource, buffer, sizeof buffer, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  callback.Reset();
  rv = PPBURLLoader()->ReadResponseBody(
      request, buffer, sizeof buffer, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  LOG_TO_BROWSER("read without open");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->ReadResponseBody(
      loader, buffer, sizeof buffer, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_FAILED == rv);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestStreamToFileSimple() {
  int32_t rv;
  PP_Resource loader;
  TestableCallback callback(pp_instance(), true);
  PP_Resource request = MakeDummyRequest();

  LOG_TO_BROWSER("stream on invalid loader");
  callback.Reset();
  rv = PPBURLLoader()->FinishStreamingToFile(
      kInvalidResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  callback.Reset();
  rv = PPBURLLoader()->FinishStreamingToFile(
      kNotAResource, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  callback.Reset();
  rv = PPBURLLoader()->FinishStreamingToFile(
      request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_BADRESOURCE == rv);

  LOG_TO_BROWSER("stream without open");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->FinishStreamingToFile(
    loader, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_FAILED == rv);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request);

  TEST_PASSED;
}


void TestAbort() {
  int32_t rv;
  PP_Resource loader;
  PP_Resource request = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                    kDontStreamToFile,
                                    kDownloadProgress,
                                    kDontFollowRedirects);
  TestableCallback callback(pp_instance(), true);

  LOG_TO_BROWSER("release loader while open in flight");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  PPBCore()->ReleaseResource(loader);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_ABORTED == rv);
  PPBCore()->ReleaseResource(loader);

  // TODO(sanga,polina): Investigate why this case is hanging
  // The problem is that the callback never gets called.
  // So WaitForResult() never returns.
  // This is also true for regular callback.
#if 0
  LOG_TO_BROWSER("close loader while open in flight");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  PPBURLLoader()->Close(loader);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_ABORTED == rv);
  PPBCore()->ReleaseResource(loader);
#endif

  // NOTE: this should be last in this function since it releases the request
  LOG_TO_BROWSER("release request while open in flight");
  loader = PPBURLLoader()->Create(pp_instance());
  callback.Reset();
  rv = PPBURLLoader()->Open(loader, request, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  PPBCore()->ReleaseResource(request);
  rv = callback.WaitForResult();
  // loader holds another ref so this is ok
  EXPECT(PP_OK == rv);
  PPBCore()->ReleaseResource(loader);

  TEST_PASSED;
}


void TestOpenReadMismatch() {
  int32_t rv;
  char buffer[1024];
  TestableCallback callback(pp_instance(), true);
  PP_Resource loader;
  PP_Resource response;
  PP_Resource request_regular = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                            kDontStreamToFile,
                                            kDownloadProgress,
                                            kFollowRedirects);
  PP_Resource request_stream = MakeRequest(kExistingURL, "GET", kNoHeaders,
                                           kStreamToFile,
                                           kDownloadProgress,
                                           kFollowRedirects);

  LOG_TO_BROWSER("open regularly but try streaming");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request_regular, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);
  PP_Resource file_ref = PPBURLResponseInfo()->GetBodyAsFileRef(response);
  EXPECT(kInvalidResource == file_ref);
  PPBCore()->ReleaseResource(loader);

  LOG_TO_BROWSER("open as stream but read via ReadResponseBody");
  callback.Reset();
  loader = PPBURLLoader()->Create(pp_instance());
  rv = PPBURLLoader()->Open(loader, request_stream, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_OK == rv);
  response = PPBURLLoader()->GetResponseInfo(loader);
  CheckResponse(response, 200, "OK", NULL, NULL);
  rv = PPBURLLoader()->ReadResponseBody(
      loader, buffer, sizeof buffer, callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == rv);
  rv = callback.WaitForResult();
  EXPECT(PP_ERROR_FAILED == rv);
  PPBCore()->ReleaseResource(loader);

  PPBCore()->ReleaseResource(request_stream);
  PPBCore()->ReleaseResource(request_regular);

  TEST_PASSED;
}

}  // namespace


void SetupTests() {
  // This test requires PPBTestingDev()
  CHECK(PPBTestingDev() != NULL);

  // TODO(robertm): add POST tests
  RegisterTest("TestOpenSimple", TestOpenSimple);
  RegisterTest("TestStreamToFileSimple", TestStreamToFileSimple);
  RegisterTest("TestProgressSimple", TestProgressSimple);
  RegisterTest("TestResponseInfoSimple", TestResponseInfoSimple);
  RegisterTest("TestCloseSimple", TestCloseSimple);
  RegisterTest("TestReadSimple", TestReadSimple);

  RegisterTest("TestAbort", TestAbort);
  RegisterTest("TestDoubleOpen", TestDoubleOpen);
  RegisterTest("TestOpenSuccess", TestOpenSuccess);
  RegisterTest("TestOpenFailure", TestOpenFailure);
  RegisterTest("TestOpenRedirect", TestOpenRedirect);
  RegisterTest("TestOpenReadMismatch", TestOpenReadMismatch);
  RegisterTest("TestStreamToFile", TestStreamToFile);
}


void SetupPluginInterfaces() {
  // No PPP interface to test.
}
