// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_test_factory.h"

#include "webkit/tools/npapi_pepper_test_plugin/pepper_3d_test.h"

namespace NPAPIClient {

PluginTest* CreatePluginTest(const std::string& test_name,
                             NPP instance,
                             NPNetscapeFuncs* host_functions) {
  PluginTest* new_test = NULL;

  if (test_name == "pepper_3d") {
    new_test = new Pepper3DTest(instance, host_functions);
  }

  return new_test;
}

} // namespace NPAPIClient
