// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowless plugin tests. Please add additional
// tests to this class.
class WindowlessPluginTest : public PluginTest {
 public:
  // Constructor.
  WindowlessPluginTest(NPP id, NPNetscapeFuncs *host_functions);

  // These tests run in windowless plugin mode.
  virtual bool IsWindowless() const;

  // NPAPI HandleEvent handler
  virtual int16 HandleEvent(void* event);

 protected:
  NPError ExecuteScript(NPNetscapeFuncs* browser, NPP id,
      const std::string& script, NPVariant* result);
  void ExecuteScriptDeleteInPaint(NPNetscapeFuncs* browser);
  void MultipleInstanceSyncCalls(NPNetscapeFuncs* browser);
  void ConvertPoint(NPNetscapeFuncs* browser);
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H_
