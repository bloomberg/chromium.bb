// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// This class tests
class DeletePluginInStreamTest : public PluginTest {
 public:
  // Constructor.
  DeletePluginInStreamTest(NPP id, NPNetscapeFuncs *host_functions);
  //
  // NPAPI functions
  //
  virtual NPError SetWindow(NPWindow* pNPWindow);
  virtual NPError NewStream(NPMIMEType type, NPStream* stream,
                            NPBool seekable, uint16* stype);
 private:
  bool test_started_;
  std::string self_url_;
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_DELETE_PLUGIN_IN_STREAM_TEST_H_
