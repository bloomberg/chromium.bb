// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppp_instance.h"

namespace {


// PostMessage is complicated in this test.  We're setting up an iframe with
// src=foo where foo is handled by an extension as a content handler.  Webkit
// generates the html page with the plugin embed, so we have no way to place
// an event handler on the plugin or body of that page that's guaranteed to
// execute before the plugin initializes.  Instead, we're just caching the
// first error (if any) we encounter during load and responding to a normal
// test message to return the results.
const int kDocLoadErrorNone = 0;
const int kDocLoadErrorInit = -1;
int error_in_doc_load_line = kDocLoadErrorInit;

#define EXPECT_ON_LOAD(condition) \
  do { \
    if (!(condition)) { \
      if (kDocLoadErrorInit == error_in_doc_load_line) { \
        error_in_doc_load_line = __LINE__; \
      } \
    } \
  } while (0);

#define ON_LOAD_PASSED \
  do { \
    if (kDocLoadErrorInit == error_in_doc_load_line) \
      error_in_doc_load_line = kDocLoadErrorNone; \
  } while (0);

// Simple 1K buffer to hold the document passed through HandleDocumentLoad.
const uint32_t kMaxFileSize = 1024;
char buffer[kMaxFileSize];
uint32_t buffer_pos = 0;

const char kKnownFileContents[] =
    "This is not an actual pdf file, just a "
    "test file so we can verify HandleDocumentLoad.";


PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  PP_Bool status = DidCreateDefault(instance, argc, argn, argv);
  printf("DidCreate! %d\n", static_cast<int>(instance));
  return status;
}

void DidDestroy(PP_Instance /* instance */) {
}

void DidChangeView(PP_Instance /* instance */,
                   const struct PP_Rect* /* position */,
                   const struct PP_Rect* /* clip */) {
}

void DidChangeFocus(PP_Instance /* instance */,
                    PP_Bool /* has_focus */) {
}

PP_Bool HandleInputEvent(PP_Instance /* instance */,
                         const struct PP_InputEvent* /* event */) {
  return PP_FALSE;
}

void ReadCallback(void* user_data, int32_t pp_error_or_bytes) {
  PP_Resource url_loader = reinterpret_cast<PP_Resource>(user_data);

  EXPECT_ON_LOAD(pp_error_or_bytes >= PP_OK);
  // EXPECT_ON_LOAD just records an error, make sure to stop processing too.
  // On error we may end up leaking the URLLoader from HandleDocumentLoad.
  if (pp_error_or_bytes < PP_OK)
    return;

  if (PP_OK == pp_error_or_bytes) {
    // Check the contents of the file against the known contents.
    int diff = strncmp(buffer,
                      kKnownFileContents,
                      strlen(kKnownFileContents));
    EXPECT_ON_LOAD(diff == 0);
    PPBURLLoader()->Close(url_loader);
    ON_LOAD_PASSED;
  } else {
    buffer_pos += pp_error_or_bytes;
    PP_CompletionCallback callback =
        PP_MakeOptionalCompletionCallback(ReadCallback, user_data);
    pp_error_or_bytes =
        PPBURLLoader()->ReadResponseBody(url_loader,
                                         buffer + buffer_pos,
                                         kMaxFileSize - buffer_pos,
                                         callback);
    if (pp_error_or_bytes != PP_OK_COMPLETIONPENDING)
      PP_RunCompletionCallback(&callback, pp_error_or_bytes);
  }
}

PP_Bool HandleDocumentLoad(PP_Instance instance,
                           PP_Resource url_loader) {
  void* user_data = reinterpret_cast<void*>(url_loader);
  PP_CompletionCallback callback =
      PP_MakeOptionalCompletionCallback(ReadCallback, user_data);
  int32_t pp_error_or_bytes = PPBURLLoader()->ReadResponseBody(url_loader,
                                                               buffer,
                                                               kMaxFileSize,
                                                               callback);
  if (pp_error_or_bytes != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, pp_error_or_bytes);
  return PP_TRUE;
}

const PPP_Instance ppp_instance_interface = {
  DidCreate,
  DidDestroy,
  DidChangeView,
  DidChangeFocus,
  HandleDocumentLoad
};

// This tests PPP_Instance::HandleDocumentLoad.
void TestHandleDocumentLoad() {
  if (error_in_doc_load_line != kDocLoadErrorNone) {
    char error[1024];
    snprintf(error, sizeof(error),
        "ERROR at %s:%d: Document Load Failed\n",
        __FILE__, error_in_doc_load_line);
    fprintf(stderr, "%s", error);
    PostTestMessage(__FUNCTION__, error);
  }
  TEST_PASSED;
}

// This tests PPB_Instance::IsFullFrame when the plugin is full frame.
// Other conditions for IsFullFrame are tested by ppb_instance tests.
void TestIsFullFrame() {
  PP_Bool full_frame = PPBInstance()->IsFullFrame(pp_instance());
  EXPECT(full_frame == PP_TRUE);
  TEST_PASSED;
}


}  // namespace


void SetupTests() {
  RegisterTest("TestHandleDocumentLoad", TestHandleDocumentLoad);
  RegisterTest("TestIsFullFrame", TestIsFullFrame);
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(PPP_INSTANCE_INTERFACE, &ppp_instance_interface);
}
