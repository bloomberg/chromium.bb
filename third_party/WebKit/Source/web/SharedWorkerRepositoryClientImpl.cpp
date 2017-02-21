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

#include "web/SharedWorkerRepositoryClientImpl.h"

#include <memory>
#include <utility>
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/SharedWorker.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebContentSecurityPolicy.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebKit.h"
#include "public/web/WebSharedWorker.h"
#include "public/web/WebSharedWorkerConnectListener.h"
#include "public/web/WebSharedWorkerCreationErrors.h"
#include "public/web/WebSharedWorkerRepositoryClient.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/PtrUtil.h"

namespace blink {

// Implementation of the callback interface passed to the embedder. This will be
// destructed when a connection to a shared worker is established.
class SharedWorkerConnectListener final
    : public WebSharedWorkerConnectListener {
 public:
  explicit SharedWorkerConnectListener(SharedWorker* worker)
      : m_worker(worker) {}

  ~SharedWorkerConnectListener() override {
    DCHECK(!m_worker->isBeingConnected());
  }

  // WebSharedWorkerConnectListener overrides.

  void workerCreated(WebWorkerCreationError creationError) override {
    m_worker->setIsBeingConnected(true);

    // No nested workers (for now) - connect() should only be called from
    // document context.
    DCHECK(m_worker->getExecutionContext()->isDocument());
    Document* document = toDocument(m_worker->getExecutionContext());
    bool isSecureContext = m_worker->getExecutionContext()->isSecureContext();
    switch (creationError) {
      case WebWorkerCreationErrorNone:
        return;
      case WebWorkerCreationErrorSecureContextMismatch:
        UseCounter::Feature feature =
            isSecureContext
                ? UseCounter::NonSecureSharedWorkerAccessedFromSecureContext
                : UseCounter::SecureSharedWorkerAccessedFromNonSecureContext;
        UseCounter::count(document, feature);
        return;
    }
  }

  void scriptLoadFailed() override {
    m_worker->dispatchEvent(Event::createCancelable(EventTypeNames::error));
    m_worker->setIsBeingConnected(false);
  }

  void connected() override { m_worker->setIsBeingConnected(false); }

  void countFeature(uint32_t feature) override {
    UseCounter::count(m_worker->getExecutionContext(),
                      static_cast<UseCounter::Feature>(feature));
  }

  Persistent<SharedWorker> m_worker;
};

static WebSharedWorkerRepositoryClient::DocumentID getId(void* document) {
  DCHECK(document);
  return reinterpret_cast<WebSharedWorkerRepositoryClient::DocumentID>(
      document);
}

void SharedWorkerRepositoryClientImpl::connect(
    SharedWorker* worker,
    WebMessagePortChannelUniquePtr port,
    const KURL& url,
    const String& name) {
  DCHECK(m_client);

  // No nested workers (for now) - connect() should only be called from document
  // context.
  DCHECK(worker->getExecutionContext()->isDocument());
  Document* document = toDocument(worker->getExecutionContext());

  // TODO(estark): this is broken, as it only uses the first header
  // when multiple might have been sent. Fix by making the
  // SharedWorkerConnectListener interface take a map that can contain
  // multiple headers.
  std::unique_ptr<Vector<CSPHeaderAndType>> headers =
      worker->getExecutionContext()->contentSecurityPolicy()->headers();
  WebString header;
  WebContentSecurityPolicyType headerType = WebContentSecurityPolicyTypeReport;

  if (headers->size() > 0) {
    header = (*headers)[0].first;
    headerType =
        static_cast<WebContentSecurityPolicyType>((*headers)[0].second);
  }

  bool isSecureContext = worker->getExecutionContext()->isSecureContext();
  std::unique_ptr<WebSharedWorkerConnectListener> listener =
      WTF::makeUnique<SharedWorkerConnectListener>(worker);
  m_client->connect(
      url, name, getId(document), header, headerType,
      worker->getExecutionContext()->securityContext().addressSpace(),
      isSecureContext ? WebSharedWorkerCreationContextTypeSecure
                      : WebSharedWorkerCreationContextTypeNonsecure,
      port.release(), std::move(listener));
}

void SharedWorkerRepositoryClientImpl::documentDetached(Document* document) {
  DCHECK(m_client);
  m_client->documentDetached(getId(document));
}

SharedWorkerRepositoryClientImpl::SharedWorkerRepositoryClientImpl(
    WebSharedWorkerRepositoryClient* client)
    : m_client(client) {}

}  // namespace blink
