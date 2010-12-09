// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_MOCK_RESOURCE_H_
#define WEBKIT_GLUE_PLUGINS_MOCK_RESOURCE_H_

#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

// Tests can derive from this to implement special test-specific resources.
// It's assumed that a test will only need one mock resource, so it can
// static_cast to get its own implementation.
class MockResource : public Resource {
 public:
  MockResource(PluginModule* module) : Resource(module) {}
  ~MockResource() {}

  virtual MockResource* AsMockResource() { return this; }
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_MOCK_RESOURCE_H_
