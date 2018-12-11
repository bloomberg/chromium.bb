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

#include "third_party/blink/renderer/core/workers/shared_worker_repository_client.h"

#include <memory>
#include <utility>
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"

namespace blink {
namespace {

mojom::SharedWorkerCreationContextType ToCreationContextType(
    bool is_secure_context) {
  return is_secure_context ? mojom::SharedWorkerCreationContextType::kSecure
                           : mojom::SharedWorkerCreationContextType::kNonsecure;
}

}  // namespace

// This is the client implementation to ServiceWorkerConnectorImpl in the
// browser process. This will be destructed when the owner document is
// destroyed.
// TODO(nhiroki): Move this class into its own file.
class SharedWorkerConnectListener final
    : public mojom::blink::SharedWorkerClient {
 public:
  explicit SharedWorkerConnectListener(SharedWorker* worker)
      : worker_(worker) {}

  ~SharedWorkerConnectListener() override {
    // We have lost our connection to the worker. If this happens before
    // Connected() is called, then it suggests that the document is gone or
    // going away.
  }

  // mojom::blink::SharedWorkerClient overrides.
  void OnCreated(
      mojom::SharedWorkerCreationContextType creation_context_type) override {
    worker_->SetIsBeingConnected(true);

    // No nested workers (for now) - connect() can only be called from a
    // document context.
    DCHECK(worker_->GetExecutionContext()->IsDocument());
    DCHECK_EQ(creation_context_type,
              ToCreationContextType(
                  worker_->GetExecutionContext()->IsSecureContext()));
  }

  void OnConnected(const Vector<mojom::WebFeature>& features_used) override {
    worker_->SetIsBeingConnected(false);
    for (auto feature : features_used)
      OnFeatureUsed(feature);
  }

  void OnScriptLoadFailed() override {
    worker_->DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
    worker_->SetIsBeingConnected(false);
  }

  void OnFeatureUsed(mojom::WebFeature feature) override {
    UseCounter::Count(worker_->GetExecutionContext(), feature);
  }

  Persistent<SharedWorker> worker_;
};

void SharedWorkerRepositoryClient::Connect(
    SharedWorker* worker,
    MessagePortChannel port,
    const KURL& url,
    mojom::blink::BlobURLTokenPtr blob_url_token,
    const String& name) {
  DCHECK(!name.IsNull());

  // Lazily bind the connector.
  if (!connector_)
    interface_provider_->GetInterface(mojo::MakeRequest(&connector_));

  // TODO(estark): this is broken, as it only uses the first header
  // when multiple might have been sent. Fix by making the
  // mojom::blink::SharedWorkerInfo take a map that can contain multiple
  // headers.
  Vector<CSPHeaderAndType> headers =
      worker->GetExecutionContext()->GetContentSecurityPolicy()->Headers();
  WebString header = "";
  auto header_type = mojom::ContentSecurityPolicyType::kReport;
  if (headers.size() > 0) {
    header = headers[0].first;
    header_type =
        static_cast<mojom::ContentSecurityPolicyType>(headers[0].second);
  }

  mojom::blink::SharedWorkerInfoPtr info(mojom::blink::SharedWorkerInfo::New(
      url, name, header, header_type,
      worker->GetExecutionContext()->GetSecurityContext().AddressSpace()));

  // No nested workers (for now) - connect() can only be called from a document
  // context.
  Document* document = To<Document>(worker->GetExecutionContext());
  mojom::blink::SharedWorkerClientPtr client_ptr;
  AddWorker(document, std::make_unique<SharedWorkerConnectListener>(worker),
            mojo::MakeRequest(&client_ptr));

  connector_->Connect(
      std::move(info), std::move(client_ptr),
      ToCreationContextType(worker->GetExecutionContext()->IsSecureContext()),
      port.ReleaseHandle(),
      mojom::blink::BlobURLTokenPtr(mojom::blink::BlobURLTokenPtrInfo(
          blob_url_token.PassInterface().PassHandle(),
          mojom::blink::BlobURLToken::Version_)));
}

void SharedWorkerRepositoryClient::DocumentDetached(Document* document) {
  // Delete any associated SharedWorkerConnectListeners, which will signal, via
  // the dropped mojo connection, disinterest in the associated shared worker.
  client_map_.erase(GetId(document));
}

SharedWorkerRepositoryClient::SharedWorkerRepositoryClient(
    service_manager::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {}

void SharedWorkerRepositoryClient::AddWorker(
    Document* document,
    std::unique_ptr<mojom::blink::SharedWorkerClient> client,
    mojom::blink::SharedWorkerClientRequest request) {
  const auto& result = client_map_.insert(GetId(document), nullptr);
  std::unique_ptr<ClientSet>& clients = result.stored_value->value;
  if (!clients)
    clients = std::make_unique<ClientSet>();
  clients->AddBinding(std::move(client), std::move(request));
}

SharedWorkerRepositoryClient::DocumentID SharedWorkerRepositoryClient::GetId(
    Document* document) {
  DCHECK(document);
  return reinterpret_cast<DocumentID>(document);
}

}  // namespace blink
