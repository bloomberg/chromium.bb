// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_

#include <memory>

#include "content/common/content_export.h"

class GURL;

namespace net {
class AuthChallengeInfo;
}

namespace network {
struct ResourceResponse;
}

namespace content {
class LoginDelegate;
class ResourceLoader;

class CONTENT_EXPORT ResourceLoaderDelegate {
 public:
  virtual std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      ResourceLoader* loader,
      const net::AuthChallengeInfo& auth_info) = 0;

  virtual bool HandleExternalProtocol(ResourceLoader* loader,
                                      const GURL& url) = 0;

  virtual void DidStartRequest(ResourceLoader* loader) = 0;
  virtual void DidReceiveRedirect(ResourceLoader* loader,
                                  const GURL& new_url,
                                  network::ResourceResponse* response) = 0;
  virtual void DidReceiveResponse(ResourceLoader* loader,
                                  network::ResourceResponse* response) = 0;

  // This method informs the delegate that the loader is done, and the loader
  // expects to be destroyed as a side-effect of this call.
  virtual void DidFinishLoading(ResourceLoader* loader) = 0;

 protected:
  virtual ~ResourceLoaderDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_LOADER_DELEGATE_H_
