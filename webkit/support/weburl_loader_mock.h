// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_WEBURL_LOADER_MOCK_H_
#define WEBKIT_SUPPORT_WEBURL_LOADER_MOCK_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoader.h"

namespace WebKit {
class WebData;
struct WebURLError;
class WebURLLoaderClient;
class WebURLRequest;
class WebURLResponse;
}

class WebURLLoaderMockFactory;

// A simple class for mocking WebURLLoader.
// If the WebURLLoaderMockFactory it is associated with has been configured to
// mock the request it gets, it serves the mocked resource.  Otherwise it just
// forwards it to the default loader.
class WebURLLoaderMock : public WebKit::WebURLLoader {
 public:
  // This object becomes the owner of |default_loader|.
  WebURLLoaderMock(WebURLLoaderMockFactory* factory,
                   WebKit::WebURLLoader* default_loader);
  virtual ~WebURLLoaderMock();

  // Simulates the asynchronous request being served.
  void ServeAsynchronousRequest(const WebKit::WebURLResponse& response,
                                const WebKit::WebData& data,
                                const WebKit::WebURLError& error);

  // Simulates the redirect being served.
  WebKit::WebURLRequest ServeRedirect(
      const WebKit::WebURLResponse& redirectResponse);

  // WebURLLoader methods:
  virtual void loadSynchronously(const WebKit::WebURLRequest& request,
                                 WebKit::WebURLResponse& response,
                                 WebKit::WebURLError& error,
                                 WebKit::WebData& data);
  virtual void loadAsynchronously(const WebKit::WebURLRequest& request,
                                  WebKit::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool defer);

  bool isDeferred() { return is_deferred_; }

 private:
  WebURLLoaderMockFactory* factory_;
  WebKit::WebURLLoaderClient* client_;
  scoped_ptr<WebKit::WebURLLoader> default_loader_;
  bool using_default_loader_;
  bool is_deferred_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderMock);
};

#endif  // WEBKIT_SUPPORT_WEBURL_LOADER_MOCK_H_
