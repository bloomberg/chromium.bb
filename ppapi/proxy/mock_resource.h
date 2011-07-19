// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MOCK_RESOURCE_H_
#define PPAPI_PROXY_MOCK_RESOURCE_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/plugin_resource.h"

namespace pp {
namespace proxy {

class MockResource : public PluginResource {
 public:
  MockResource(const HostResource& resource);
  virtual ~MockResource();

  // Resource overrides.
  virtual MockResource* AsMockResource();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockResource);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_MOCK_RESOURCE_H_
