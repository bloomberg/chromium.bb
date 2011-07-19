// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/mock_resource.h"

namespace pp {
namespace proxy {

MockResource::MockResource(const HostResource& resource)
    : PluginResource(resource) {
}

MockResource::~MockResource() {
}

MockResource* MockResource::AsMockResource() {
  return this;
}

}  // namespace proxy
}  // namespace pp
