// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_THREAD_ASYNC_CALL_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_THREAD_ASYNC_CALL_TEST_H_

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace base {
class ShadowingAtExitManager;
}

namespace NPAPIClient {

// This class tests scheduling and unscheduling of async callbacks using
// NPN_PluginThreadAsyncCall.
class PluginThreadAsyncCallTest : public PluginTest {
 public:
  PluginThreadAsyncCallTest(NPP id, NPNetscapeFuncs *host_functions);
  virtual ~PluginThreadAsyncCallTest();

  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved) OVERRIDE;

  virtual NPError Destroy() OVERRIDE;

  void AsyncCall();
  void OnCallSucceeded();
  void OnCallCompleted();

 private:
  // base::Thread needs one of these.
  base::ShadowingAtExitManager* at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(PluginThreadAsyncCallTest);
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_THREAD_ASYNC_CALL_TEST_H_
