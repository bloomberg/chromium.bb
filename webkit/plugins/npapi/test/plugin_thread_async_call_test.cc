// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define UNIT_TEST  // To get the ShadowingAtExitManager.
#include "base/at_exit.h"

#include "webkit/plugins/npapi/test/plugin_thread_async_call_test.h"

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "webkit/plugins/npapi/test/plugin_client.h"

namespace NPAPIClient {

namespace {

// There are two plugin instances in this test. The long lived instance is used
// for reporting errors and signalling test completion. The short lived one is
// used to verify that async callbacks are not invoked after NPP_Destroy.
PluginThreadAsyncCallTest* g_short_lived_instance;
PluginThreadAsyncCallTest* g_long_lived_instance;

void OnCallSucceededHelper(void* data) {
  static_cast<PluginThreadAsyncCallTest*>(data)->OnCallSucceeded();
}

class AsyncCallTask : public Task {
 public:
  AsyncCallTask(PluginThreadAsyncCallTest* test_class)
    : test_class_(test_class) {}

  void Run() {
    test_class_->AsyncCall();
  }

 private:
  PluginThreadAsyncCallTest* test_class_;
};

void OnCallFailed(void* data) {
  g_long_lived_instance->SetError("Async callback invoked after NPP_Destroy");
}

void OnCallCompletedHelper(void* data) {
  static_cast<PluginThreadAsyncCallTest*>(data)->OnCallCompleted();
}
}

PluginThreadAsyncCallTest::PluginThreadAsyncCallTest(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions), at_exit_manager_(NULL) {
}

PluginThreadAsyncCallTest::~PluginThreadAsyncCallTest() {
  delete at_exit_manager_;
}

NPError PluginThreadAsyncCallTest::New(
    uint16 mode, int16 argc, const char* argn[], const char* argv[],
    NPSavedData* saved) {
  NPError error = PluginTest::New(mode, argc, argn, argv, saved);
  if (error != NPERR_NO_ERROR)
    return error;

  // Determine whether this is the short lived instance.
  for (int i = 0; i < argc; ++i) {
    if (base::strcasecmp(argn[i], "short_lived") == 0) {
      if (base::strcasecmp(argv[i], "true") == 0) {
        g_short_lived_instance = this;
      } else {
        g_long_lived_instance = this;
      }
    }
  }

  // Schedule an async call that will succeed.  Make sure to call that API from
  // a different thread to fully test it.
  if (this == g_short_lived_instance) {
    // This is slightly complicated thanks to the Linux shared library build,
    // which shares more compilation units between the NPAPI plug-in and
    // the base code.
    at_exit_manager_ = new base::ShadowingAtExitManager();
    base::Thread random_thread("random_thread");
    random_thread.Start();
    random_thread.message_loop()->PostTask(FROM_HERE, new AsyncCallTask(this));
  }

  return NPERR_NO_ERROR;
}

void PluginThreadAsyncCallTest::AsyncCall() {
  HostFunctions()->pluginthreadasynccall(id(), OnCallSucceededHelper, this);
}

void PluginThreadAsyncCallTest::OnCallSucceeded() {
  // Delete the short lived instance.
  NPIdentifier delete_id = HostFunctions()->getstringidentifier(
      "deleteShortLivedInstance");

  NPObject *window_obj = NULL;
  HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window_obj);

  NPVariant result;
  HostFunctions()->invoke(id(), window_obj, delete_id, NULL, 0, &result);
}

NPError PluginThreadAsyncCallTest::Destroy() {
  if (this == g_short_lived_instance) {
    // Schedule an async call that should not be called.
    HostFunctions()->pluginthreadasynccall(id(), OnCallFailed, NULL);

    // Schedule an async call to end the test using the long lived instance.
    HostFunctions()->pluginthreadasynccall(g_long_lived_instance->id(),
                                           OnCallCompletedHelper,
                                           g_long_lived_instance);
  }

  return NPERR_NO_ERROR;
}

void PluginThreadAsyncCallTest::OnCallCompleted() {
  SignalTestCompleted();
}

}  // namespace NPAPIClient
