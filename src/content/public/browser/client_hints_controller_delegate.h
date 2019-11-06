// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"

class GURL;

namespace blink {
struct WebEnabledClientHints;
struct UserAgentMetadata;
}  // namespace blink

namespace network {
class NetworkQualityTracker;

}

namespace content {

class CONTENT_EXPORT ClientHintsControllerDelegate {
 public:
  virtual ~ClientHintsControllerDelegate() = default;

  virtual network::NetworkQualityTracker* GetNetworkQualityTracker() = 0;

  // Get which client hints opt-ins were persisted on current origin.
  virtual void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) = 0;

  virtual bool IsJavaScriptAllowed(const GURL& url) = 0;

  virtual std::string GetAcceptLanguageString() = 0;

  virtual blink::UserAgentMetadata GetUserAgentMetadata() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
