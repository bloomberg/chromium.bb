// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/weburlrequest_extradata_impl.h"

using WebKit::WebReferrerPolicy;

namespace webkit_glue {

WebURLRequestExtraDataImpl::WebURLRequestExtraDataImpl(
    WebReferrerPolicy referrer_policy)
    : referrer_policy_(referrer_policy) {
}

WebURLRequestExtraDataImpl::~WebURLRequestExtraDataImpl() {
}

}  // namespace webkit_glue
