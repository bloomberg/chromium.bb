// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/child/weburlrequest_extradata_impl.h"

using WebKit::WebReferrerPolicy;
using WebKit::WebString;

namespace webkit_glue {

WebURLRequestExtraDataImpl::WebURLRequestExtraDataImpl(
    WebReferrerPolicy referrer_policy,
    const WebString& custom_user_agent,
    bool was_after_preconnect_request)
    : referrer_policy_(referrer_policy),
      custom_user_agent_(custom_user_agent),
      was_after_preconnect_request_(was_after_preconnect_request) {
}

WebURLRequestExtraDataImpl::~WebURLRequestExtraDataImpl() {
}

}  // namespace webkit_glue
