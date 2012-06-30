// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/resource_host.h"

#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace host {

ResourceHost::ResourceHost(PpapiHost* host,
                           PP_Instance instance,
                           PP_Resource resource)
    : host_(host),
      pp_instance_(instance),
      pp_resource_(resource) {
}

ResourceHost::~ResourceHost() {
}

int32_t ResourceHost::OnResourceMessageReceived(const IPC::Message& msg,
                                                HostMessageContext* context) {
  return PP_ERROR_NOTSUPPORTED;
}

}  // namespace host
}  // namespace ppapi
