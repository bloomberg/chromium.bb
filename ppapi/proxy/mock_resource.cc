// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/mock_resource.h"

namespace ppapi {
namespace proxy {

MockResource::MockResource(const HostResource& resource) : Resource(resource) {
}

MockResource::~MockResource() {
}

}  // namespace proxy
}  // namespace ppapi
