// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/weburlrequest_extradata_impl.h"

using WebKit::WebReferrerPolicy;
using WebKit::WebString;

namespace webkit_glue {

WebURLRequestExtraDataImpl::WebURLRequestExtraDataImpl(
    WebReferrerPolicy referrer_policy,
    const WebString& custom_user_agent)
    : referrer_policy_(referrer_policy),
      custom_user_agent_(custom_user_agent) {
}

WebURLRequestExtraDataImpl::~WebURLRequestExtraDataImpl() {
}

}  // namespace webkit_glue
