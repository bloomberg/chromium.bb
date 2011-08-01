// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests PPB_URLRequestInfo.

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "native_client/tests/ppapi_test_lib/testable_callback.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"

namespace {

// Tests
//   PP_Resource Create(PP_Instance instance).
void TestCreate() {
  PP_Resource url_request = kInvalidResource;

  // Invalid instance -> invalid resource.
  url_request = PPBURLRequestInfo()->Create(kInvalidInstance);
  EXPECT(url_request == kInvalidResource);

  // Valid instance -> valid resource.
  url_request = PPBURLRequestInfo()->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  PPBCore()->ReleaseResource(url_request);
  TEST_PASSED;
}

// Tests
//   PP_Bool IsURLRequestInfo(PP_Resource resource).
void TestIsURLRequestInfo() {
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();

  // Invalid / non-existent / non-URLRequestInfo resource -> false.
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(kInvalidResource));
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(kNotAResource));
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  EXPECT(url_loader != kInvalidResource);
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_loader));
  PPBCore()->ReleaseResource(url_loader);

  // Current URLRequestInfo resource -> true.
  PP_Resource url_request = ppb->Create(pp_instance());
  EXPECT(PP_TRUE == ppb->IsURLRequestInfo(url_request));

  // Released URLRequestInfo resource -> false.
  PPBCore()->ReleaseResource(url_request);
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_request));

  TEST_PASSED;
}

// Tests
//  PP_Bool SetProperty(PP_Resource request,
//                      PP_URLRequestProperty property,
//                      struct PP_Var value);
void TestSetProperty() {
  struct PropertyTestData {
    PropertyTestData(PP_URLRequestProperty _property,
                     const std::string& _property_name,
                     PP_Var _var, PP_Bool _expected_value) :
                         property(_property), property_name(_property_name),
                         var(_var), expected_value(_expected_value) {
      PPBVar()->AddRef(var);
    }
    PP_URLRequestProperty property;
    std::string property_name;
    PP_Var var;
    PP_Bool expected_value;
  };

  // All bool properties should accept PP_TRUE and PP_FALSE, while rejecting
  // all other variable types.
  #define ADD_BOOL_PROPERTY(_prop_name) \
    PropertyTestData(ID_STR(_prop_name), PP_MakeBool(PP_TRUE), PP_TRUE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeBool(PP_FALSE), PP_TRUE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeUndefined(), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeNull(), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeInt32(0), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeDouble(0.0), PP_FALSE)

  // These property types are always invalid for string properties.
  #define ADD_STRING_INVALID_PROPERTIES(_prop_name) \
    PropertyTestData(ID_STR(_prop_name), PP_MakeNull(), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeBool(PP_FALSE), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeInt32(0), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeDouble(0.0), PP_FALSE)

  #define ADD_INT_INVALID_PROPERTIES(_prop_name) \
    PropertyTestData(ID_STR(_prop_name), PP_MakeUndefined(), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeNull(), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeBool(PP_FALSE), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeString(""), PP_FALSE), \
    PropertyTestData(ID_STR(_prop_name), PP_MakeDouble(0.0), PP_FALSE)

  #define ID_STR(arg) arg, #arg
  PropertyTestData test_data[] = {
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_STREAMTOFILE),
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS),
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS),
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS),
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS),
      ADD_BOOL_PROPERTY(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS),
      ADD_STRING_INVALID_PROPERTIES(PP_URLREQUESTPROPERTY_URL),
      ADD_STRING_INVALID_PROPERTIES(PP_URLREQUESTPROPERTY_METHOD),
      ADD_STRING_INVALID_PROPERTIES(PP_URLREQUESTPROPERTY_HEADERS),
      ADD_STRING_INVALID_PROPERTIES(
          PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
      ADD_STRING_INVALID_PROPERTIES(
          PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
      ADD_INT_INVALID_PROPERTIES(
          PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
      ADD_INT_INVALID_PROPERTIES(
          PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
          PP_MakeString("http://www.google.com"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
          PP_MakeString("foo.jpg"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
          PP_MakeString("GET"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
          PP_MakeString("POST"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString("Accept: text/plain"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString(""), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
          PP_MakeString("http://www.google.com"), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
          PP_MakeString(""), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL),
          PP_MakeUndefined(), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString("base64"), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString(""), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeUndefined(), PP_TRUE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
          PP_MakeUndefined(), PP_FALSE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
          PP_MakeUndefined(), PP_FALSE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString("Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA=="),
          PP_FALSE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_HEADERS),
          PP_MakeString("Accept-Encoding: *\n"
                        "Accept-Charset: iso-8859-5, unicode-1-1;q=0.8"),
          PP_FALSE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(0), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(100), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(0), PP_TRUE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(100), PP_TRUE),
      // Bug filed against Chrome to validate SetProperty values better.
      // http://code.google.com/p/chromium/issues/detail?id=89842.
      /*
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_URL),
          PP_MakeString("::::::::::::"), PP_FALSE),
      PropertyTestData(ID_STR(PP_URLREQUESTPROPERTY_METHOD),
          PP_MakeString("INVALID"), PP_FALSE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING),
          PP_MakeString("invalid"), PP_FALSE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD),
          PP_MakeInt32(-100), PP_FALSE),
      PropertyTestData(
          ID_STR(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD),
          PP_MakeInt32(-100), PP_TRUE),
      */
  };

  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();
  PP_Resource url_request = ppb->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  for (size_t i = 0;
       i < sizeof(test_data)/sizeof(PropertyTestData);
       ++i) {
    if (test_data[i].expected_value !=
            ppb->SetProperty(url_request,
                             test_data[i].property,
                             test_data[i].var)) {
        nacl::string error_string = nacl::string("Setting property ") +
            test_data[i].property_name.c_str() + " to " +
            StringifyVar(test_data[i].var) + " did not return " +
            StringifyVar(PP_MakeBool(test_data[i].expected_value));
        // PostTestMessage will signal test failure here.
        PostTestMessage(__FUNCTION__, error_string.c_str());
    }
    PPBVar()->Release(test_data[i].var);
  }

  PPBCore()->ReleaseResource(url_request);
  TEST_PASSED;
}


void LoadAndCompareBody(PP_Resource request,
                        const nacl::string& expected_body) {
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  EXPECT(url_loader != kInvalidResource);

  TestableCallback callback(pp_instance(), true);

  int32_t result = PPBURLLoader()->Open(url_loader,
                                        request,
                                        callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == result);
  result = callback.WaitForResult();
  EXPECT(PP_OK == result);

  PP_Resource response = PPBURLLoader()->GetResponseInfo(url_loader);
  EXPECT(response != kInvalidResource);

  PP_Var status =
      PPBURLResponseInfo()->GetProperty(response,
                                        PP_URLRESPONSEPROPERTY_STATUSCODE);
  EXPECT_VAR_INT(status, 200);

  nacl::string actual_body;

  // Read the entire body in this loop.
  for (;;) {
    callback.Reset();
    const size_t kBufferSize = 32;
    char buf[kBufferSize];
    result = PPBURLLoader()->ReadResponseBody(url_loader,
                                              buf, kBufferSize,
                                              callback.GetCallback());
    EXPECT(PP_OK_COMPLETIONPENDING == result);
    result = callback.WaitForResult();
    EXPECT(result >= PP_OK);
    if (result < 0) {
      return;
    }
    if (PP_OK == result) {
      break;
    }
    actual_body.append(buf, result);
  }

  EXPECT(actual_body == expected_body);

  PPBCore()->ReleaseResource(response);
  PPBCore()->ReleaseResource(url_loader);
}

void SetupPOSTURLRequest(PP_Resource request) {
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();
  EXPECT(PP_TRUE == ppb->SetProperty(request,
                                     PP_URLREQUESTPROPERTY_METHOD,
                                     PP_MakeString("POST")));
  EXPECT(PP_TRUE == ppb->SetProperty(request,
                                     PP_URLREQUESTPROPERTY_URL,
                                     PP_MakeString("/echo")));
}

void TestAppendDataToBody() {
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();
  PP_Resource url_request = ppb->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);
  nacl::string postdata("sample postdata");

  // Bad pointer passed in should fail to proxy.
  EXPECT(PP_FALSE == ppb->AppendDataToBody(url_request,
                                           NULL,
                                           1));
  // Invalid resource should fail.
  EXPECT(PP_FALSE == ppb->AppendDataToBody(kInvalidResource,
                                          postdata.data(),
                                          postdata.length()));
  // Append data and POST to echoing web server.
  SetupPOSTURLRequest(url_request);
  EXPECT(PP_TRUE == ppb->AppendDataToBody(url_request,
                                          postdata.data(),
                                          postdata.length()));
  // Check for success.
  LoadAndCompareBody(url_request, postdata);

  PPBCore()->ReleaseResource(url_request);
  TEST_PASSED;
}

// TODO(elijahtaylor): Revisit this function when it is enabled. Parts could be
// factored out and it could be smaller and more readable in general.
void TestAppendFileToBody() {
  const int64_t kFileSystemSize = 1024;
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();
  PP_Resource url_request = ppb->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  // Creating a file reference PP_Resource is a multi-stage process.
  // First, create and open a new file system.
  const PPB_FileSystem* ppb_file_sys = PPBFileSystem();
  PP_Resource file_sys =
      ppb_file_sys->Create(pp_instance(),
                           PP_FILESYSTEMTYPE_EXTERNAL);
  EXPECT(file_sys != kInvalidResource);

  TestableCallback callback(pp_instance(), true);

  int32_t result = ppb_file_sys->Open(file_sys,
                                      kFileSystemSize,
                                      callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == result);
  result = callback.WaitForResult();

  EXPECT(PP_OK == result);

  // Create a file reference, then tie it to a new writable file io resource.
  PP_Resource file_ref = PPBFileRef()->Create(file_sys, "/foo");
  EXPECT(file_ref != kInvalidResource);
  PP_Resource file_io = PPBFileIO()->Create(pp_instance());
  EXPECT(file_ref != kInvalidResource);

  callback.Reset();
  result = PPBFileIO()->Open(file_io,
                             file_ref,
                             PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
                             PP_FILEOPENFLAG_TRUNCATE,
                             callback.GetCallback());

  EXPECT(PP_OK_COMPLETIONPENDING == result);
  result = callback.WaitForResult();
  EXPECT(PP_OK == result);

  // Now we can write the contents of this string to the file io resource.
  nacl::string postdata("sample postdata from file");
  size_t total_written = 0;
  for (;;) {
    callback.Reset();
    result = PPBFileIO()->Write(file_io,
                                total_written,
                                postdata.data() + total_written,
                                postdata.length() - total_written,
                                callback.GetCallback());

    EXPECT(PP_OK_COMPLETIONPENDING == result);
    result = callback.WaitForResult();

    EXPECT(result >= PP_OK);
    if (result < 0)
      return;
    if (result == PP_OK)
      break;
    total_written += result;
  }
  PPBFileIO()->Close(file_io);

  const int64_t kReadUntilEOF = -1;
  const PP_Time kDummyTimeValue = 5;  // seconds since epoch

  // Finally, set up the url_request to post to an echoing web server.
  SetupPOSTURLRequest(url_request);
  EXPECT(PP_TRUE == ppb->AppendFileToBody(url_request,
                                          file_ref,
                                          0,    // start offset
                                          postdata.length(),
                                          0));  // expected last modified time
  LOG_TO_BROWSER("testing ordinary 'AppendFileToBody'");
  // Check for success.
  LoadAndCompareBody(url_request, postdata);

  // Use a new url_request for another POST request
  PPBCore()->ReleaseResource(url_request);
  url_request = ppb->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  callback.Reset();
  // Set the timestamps so we can test 'expected last modified time'
  result = PPBFileIO()->Touch(file_ref,
                              kDummyTimeValue+1,
                              kDummyTimeValue,
                              callback.GetCallback());

  SetupPOSTURLRequest(url_request);
  EXPECT(PP_TRUE == ppb->AppendFileToBody(url_request,
                                          file_ref,
                                          0,  // start offset
                                          postdata.length(),
                                          kDummyTimeValue));
  LOG_TO_BROWSER("testing 'expected last modified time'");
  LoadAndCompareBody(url_request, postdata);

  PPBCore()->ReleaseResource(url_request);
  url_request = ppb->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  SetupPOSTURLRequest(url_request);
  // Test for failure on expected last modified time, it won't fail until an
  // actual request is made.
  EXPECT(PP_TRUE == ppb->AppendFileToBody(url_request,
                                          file_ref,
                                          0,  // start offset
                                          kReadUntilEOF,  // number of bytes
                                          kDummyTimeValue - 1));
  LOG_TO_BROWSER("testing 'expected last modified time' failure");
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  EXPECT(url_loader != kInvalidResource);
  callback.Reset();

  result = PPBURLLoader()->Open(url_loader,
                                url_request,
                                callback.GetCallback());
  EXPECT(PP_OK_COMPLETIONPENDING == result);
  result = callback.WaitForResult();
  EXPECT(PP_ERROR_FILECHANGED == result);

  // Invalid Resource is failure.
  EXPECT(PP_FALSE == ppb->AppendFileToBody(url_request,
                                           kInvalidResource,
                                           0,    // start offset
                                           1,    // length
                                           0));  // expected last modified time
  // Test bad start offset.
  EXPECT(PP_FALSE == ppb->AppendFileToBody(url_request,
                                           file_ref,
                                           -1,   // start offset
                                           1,    // length
                                           0));  // expected last modified time
  // Test invalid length. (-1 means read until end of file, hence -2)
  EXPECT(PP_FALSE == ppb->AppendFileToBody(url_request,
                                           kInvalidResource,
                                           0,    // start offset
                                           -2,   // length
                                           0));  // expected last modified time
  PPBCore()->ReleaseResource(url_request);
  PPBCore()->ReleaseResource(file_ref);
  PPBCore()->ReleaseResource(file_sys);
  TEST_PASSED;
}

// Allocates and manipulates a large number of resources.
void TestStress() {
  const int kManyResources = 500;
  PP_Resource url_request_info[kManyResources];
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();

  for (int i = 0; i < kManyResources; i++) {
    url_request_info[i] = ppb->Create(pp_instance());
    EXPECT(url_request_info[i] != kInvalidResource);
    EXPECT(PP_TRUE == ppb->IsURLRequestInfo(url_request_info[i]));
    EXPECT(PP_TRUE == ppb->SetProperty(url_request_info[i],
                                       PP_URLREQUESTPROPERTY_STREAMTOFILE,
                                       PP_MakeBool(PP_FALSE)));
  }
  for (int i = 0; i < kManyResources; i++) {
    PPBCore()->ReleaseResource(url_request_info[i]);
    EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_request_info[i]));
  }

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsURLRequestInfo", TestIsURLRequestInfo);
  RegisterTest("TestSetProperty", TestSetProperty);
  RegisterTest("TestAppendDataToBody", TestAppendDataToBody);
  RegisterTest("TestAppendFileToBody", TestAppendFileToBody);
  RegisterTest("TestStress", TestStress);
}

void SetupPluginInterfaces() {
  // none
}
