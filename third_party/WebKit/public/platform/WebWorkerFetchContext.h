// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebWorkerFetchContext_h
#define WebWorkerFetchContext_h

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class WebURLLoader;
class WebURLRequest;

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
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader() = 0;

  // Called when a request is about to be sent out to modify the request to
  // handle the request correctly in the loading stack later. (Example: service
  // worker)
  virtual void WillSendRequest(WebURLRequest&) = 0;

  // Whether the fetch context is controlled by a service worker.
  virtual bool IsControlledByServiceWorker() const = 0;
};

}  // namespace blink

#endif  // WebWorkerFetchContext_h
