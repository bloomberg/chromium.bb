// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_DELEGATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_DELEGATE_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include <memory>

namespace network {
struct ResourceRequest;
}

namespace blink {
class WebRequestPeer;
class WebResourceRequestSender;
class WebString;
class WebURL;

// Interface that allows observing request events and optionally replacing
// the peer. Note that if it doesn't replace the peer it must return the
// current peer so that the ownership is continued to be held by
// WebResourceRequestSender.
class BLINK_PLATFORM_EXPORT WebResourceRequestSenderDelegate {
 public:
  virtual ~WebResourceRequestSenderDelegate() = default;

  virtual void OnRequestComplete() = 0;

  // Note that |url| is the final values (e.g. after any redirects).
  virtual scoped_refptr<WebRequestPeer> OnReceivedResponse(
      scoped_refptr<WebRequestPeer> current_peer,
      const WebString& mime_type,
      const WebURL& url) = 0;

  virtual bool CanHandleURL(const std::string& url) { return false; }

  virtual std::unique_ptr<WebNavigationBodyLoader>
      CreateBodyLoader(network::ResourceRequest* request) { return nullptr; }

  virtual void Start(WebRequestPeer* peer,
                     WebResourceRequestSender* sender,
                     network::ResourceRequest* request,
                     int request_id,
                     scoped_refptr<base::SingleThreadTaskRunner> runner) {}

  virtual void Cancel(int request_id) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_REQUEST_SENDER_DELEGATE_H_
