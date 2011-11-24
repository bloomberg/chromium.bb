// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLREQUEST_EXTRADATA_IMPL_H_
#define WEBKIT_GLUE_WEBURLREQUEST_EXTRADATA_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebReferrerPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

namespace webkit_glue {

// Base class for Chrome's implementation of the "extra data" stored in each
// ResourceRequest.
class WebURLRequestExtraDataImpl : public WebKit::WebURLRequest::ExtraData {
 public:
  WebURLRequestExtraDataImpl(WebKit::WebReferrerPolicy referrer_policy);
  virtual ~WebURLRequestExtraDataImpl();

  WebKit::WebReferrerPolicy referrer_policy() const { return referrer_policy_; }

 private:
  WebKit::WebReferrerPolicy referrer_policy_;

  DISALLOW_COPY_AND_ASSIGN(WebURLRequestExtraDataImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBURLREQUEST_EXTRADATA_IMPL_H_
