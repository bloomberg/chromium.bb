// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_PLUGINS_NPAPI_TEST_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// This test verifies that if a plugin scripts the page from within an
// NPObject's deallocation, and the page tears down the plugin, then that
// object doesn't end up being double-freed.

class DeletePluginInDeallocateTest : public PluginTest {
 public:
  // Constructor.
  DeletePluginInDeallocateTest(NPP id, NPNetscapeFuncs* host_functions);

  // NPAPI handlers.
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError Destroy();

  NPObject* npobject_;
  bool test_started_;
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_DELETE_PLUGIN_IN_DEALLOCATE_TEST_H_
