// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_MOCK_RESOURCE_H_
#define WEBKIT_PLUGINS_PPAPI_MOCK_RESOURCE_H_

#include "ppapi/shared_impl/resource.h"

namespace webkit {
namespace ppapi {

// Tests can derive from this to implement special test-specific resources.
// It's assumed that a test will only need one mock resource, so it can
// static_cast to get its own implementation.
class MockResource : public ::ppapi::Resource {
 public:
  MockResource(PP_Instance instance) : Resource(instance) {}
  virtual ~MockResource() {}
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_MOCK_RESOURCE_H_
