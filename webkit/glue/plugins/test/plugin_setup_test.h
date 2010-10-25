// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_SETUP_TEST_H__
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_SETUP_TEST_H__

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// A very simple test that just sets up a new plug-in.
class PluginSetupTest : public PluginTest {
 public:
  // Constructor.
  PluginSetupTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_SETUP_TEST_H__
