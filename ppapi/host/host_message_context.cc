// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/host_message_context.h"

namespace ppapi {
namespace host {

HostMessageContext::HostMessageContext(
    const ppapi::proxy::ResourceMessageCallParams& cp)
    : params(cp) {
}

HostMessageContext::~HostMessageContext() {
}

ppapi::proxy::ResourceMessageReplyParams
HostMessageContext::MakeReplyParams() const {
  return ppapi::proxy::ResourceMessageReplyParams(params.pp_resource(),
                                                  params.sequence());
}

}  // namespace host
}  // namespace ppapi
