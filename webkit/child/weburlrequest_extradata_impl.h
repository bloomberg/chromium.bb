// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBURLREQUEST_EXTRADATA_IMPL_H_
#define WEBKIT_CHILD_WEBURLREQUEST_EXTRADATA_IMPL_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "webkit/child/webkit_child_export.h"

namespace webkit_glue {

// Base class for Chrome's implementation of the "extra data" stored in each
// ResourceRequest.
class WEBKIT_CHILD_EXPORT WebURLRequestExtraDataImpl :
    public NON_EXPORTED_BASE(WebKit::WebURLRequest::ExtraData) {
 public:
  // |custom_user_agent| is used to communicate an overriding custom user agent
  // to |RenderViewImpl::willSendRequest()|; set to a null string to indicate no
  // override and an empty string to indicate that there should be no user
  // agent. This needs to be here, instead of content's |RequestExtraData| since
  // ppb_url_request_info_impl.cc needs to be able to set it.
  explicit WebURLRequestExtraDataImpl(
      WebKit::WebReferrerPolicy referrer_policy,
      const WebKit::WebString& custom_user_agent,
      bool was_after_preconnect_request);
  virtual ~WebURLRequestExtraDataImpl();

  WebKit::WebReferrerPolicy referrer_policy() const { return referrer_policy_; }
  const WebKit::WebString& custom_user_agent() const {
    return custom_user_agent_;
  }
  bool was_after_preconnect_request() { return was_after_preconnect_request_; }

 private:
  WebKit::WebReferrerPolicy referrer_policy_;
  WebKit::WebString custom_user_agent_;
  bool was_after_preconnect_request_;

  DISALLOW_COPY_AND_ASSIGN(WebURLRequestExtraDataImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBURLREQUEST_EXTRADATA_IMPL_H_
