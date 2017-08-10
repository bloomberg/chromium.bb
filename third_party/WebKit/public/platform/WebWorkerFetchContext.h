// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebWorkerFetchContext_h
#define WebWorkerFetchContext_h

#include <memory>

#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebURL.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class WebURLLoader;
class WebURLRequest;
class WebDocumentSubresourceFilter;

// WebWorkerFetchContext is a per-worker object created on the main thread,
// passed to a worker (dedicated, shared and service worker) and initialized on
// the worker thread by InitializeOnWorkerThread(). It contains information
// about the resource fetching context (ex: service worker provider id), and is
// used to create a new WebURLLoader instance in the worker thread.
class WebWorkerFetchContext {
 public:
  virtual ~WebWorkerFetchContext() {}

  virtual void InitializeOnWorkerThread(base::SingleThreadTaskRunner*) = 0;

  // Returns a new WebURLLoader instance which is associated with the worker
  // thread.
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      base::SingleThreadTaskRunner*) = 0;

  // Called when a request is about to be sent out to modify the request to
  // handle the request correctly in the loading stack later. (Example: service
  // worker)
  virtual void WillSendRequest(WebURLRequest&) = 0;

  // Whether the fetch context is controlled by a service worker.
  virtual bool IsControlledByServiceWorker() const = 0;

  // The flag for Data Saver.
  virtual void SetDataSaverEnabled(bool) = 0;
  virtual bool IsDataSaverEnabled() const = 0;

  // This flag is used to block all mixed content in subframes.
  virtual void SetIsOnSubframe(bool) {}
  virtual bool IsOnSubframe() const { return false; }

  // The URL that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  // See content::URLRequest::site_for_cookies() for details.
  virtual WebURL SiteForCookies() const = 0;

  // Reports the certificate error to the browser process.
  virtual void DidRunContentWithCertificateErrors(const WebURL& url) {}
  virtual void DidDisplayContentWithCertificateErrors(const WebURL& url) {}

  // Reports that the security origin has run active content from an insecure
  // source.
  virtual void DidRunInsecureContent(const WebSecurityOrigin&,
                                     const WebURL& insecure_url) {}

  virtual void SetApplicationCacheHostID(int id) {}
  virtual int ApplicationCacheHostID() const {
    return WebApplicationCacheHost::kAppCacheNoHostId;
  }

  // Sets the builder object of WebDocumentSubresourceFilter on the main thread
  // which will be used in TakeSubresourceFilter() to create a
  // WebDocumentSubresourceFilter on the worker thread.
  virtual void SetSubresourceFilterBuilder(
      std::unique_ptr<WebDocumentSubresourceFilter::Builder>) {}

  // Creates a WebDocumentSubresourceFilter on the worker thread using the
  // WebDocumentSubresourceFilter::Builder which is set on the main thread.
  // This method should only be called once.
  virtual std::unique_ptr<WebDocumentSubresourceFilter>
  TakeSubresourceFilter() {
    return nullptr;
  }
};

}  // namespace blink

#endif  // WebWorkerFetchContext_h
