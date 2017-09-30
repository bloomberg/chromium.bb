/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSharedWorkerClient_h
#define WebSharedWorkerClient_h

#include "public/platform/WebContentSettingsClient.h"
#include "public/platform/WebWorkerFetchContext.h"
#include "public/platform/web_feature.mojom-shared.h"
#include "public/web/WebDevToolsAgentClient.h"

namespace blink {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebNotificationPresenter;
class WebServiceWorkerNetworkProvider;
class WebString;

// Provides an interface back to the in-page script object for a worker.
// All functions are expected to be called back on the thread that created
// the Worker object, unless noted.
// An instance of this class must outlive or must have the identical lifetime
// as WebSharedWorker (i.e. must be kept alive until workerScriptLoadFailed()
// or workerContextDestroyed() is called).
class WebSharedWorkerClient {
 public:
  virtual void CountFeature(mojom::WebFeature) = 0;
  virtual void WorkerContextClosed() = 0;
  virtual void WorkerContextDestroyed() = 0;
  virtual void WorkerReadyForInspection() {}
  virtual void WorkerScriptLoaded() = 0;
  virtual void WorkerScriptLoadFailed() = 0;
  virtual void SelectAppCacheID(long long) = 0;

  // Returns the notification presenter for this worker context. Pointer
  // is owned by the object implementing WebSharedWorkerClient.
  virtual WebNotificationPresenter* NotificationPresenter() = 0;

  // Called on the main webkit thread in the worker process during
  // initialization.
  virtual std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) = 0;

  // Called on the main thread during initialization.
  virtual std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() = 0;

  virtual void SendDevToolsMessage(int session_id,
                                   int call_id,
                                   const WebString& message,
                                   const WebString& state) {}
  virtual WebDevToolsAgentClient::WebKitClientMessageLoop*
  CreateDevToolsMessageLoop() {
    return nullptr;
  }

  // Returns a new WebWorkerFetchContext for the shared worker. Ownership of the
  // returned object is transferred to the caller. This is used only when
  // off-main-thread-fetch is enabled.
  virtual std::unique_ptr<WebWorkerFetchContext> CreateWorkerFetchContext(
      WebServiceWorkerNetworkProvider*) {
    return nullptr;
  }
};

}  // namespace blink

#endif
