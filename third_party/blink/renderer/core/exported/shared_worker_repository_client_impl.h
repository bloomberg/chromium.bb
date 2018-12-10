/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_SHARED_WORKER_REPOSITORY_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_SHARED_WORKER_REPOSITORY_CLIENT_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/workers/shared_worker_repository_client.h"

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace blink {

// TODO(nhiroki): Merge this file into
// core/workers/shared_worker_repository_client.h.
class CORE_EXPORT SharedWorkerRepositoryClientImpl final
    : public SharedWorkerRepositoryClient {
  USING_FAST_MALLOC(SharedWorkerRepositoryClientImpl);

 public:
  static std::unique_ptr<SharedWorkerRepositoryClientImpl> Create(
      service_manager::InterfaceProvider* interface_provider) {
    return base::WrapUnique(
        new SharedWorkerRepositoryClientImpl(interface_provider));
  }

  ~SharedWorkerRepositoryClientImpl() override = default;

  void Connect(SharedWorker*,
               MessagePortChannel,
               const KURL&,
               mojom::blink::BlobURLTokenPtr,
               const String& name) override;
  void DocumentDetached(Document*) override;

 private:
  explicit SharedWorkerRepositoryClientImpl(
      service_manager::InterfaceProvider*);

  void AddWorker(Document* document,
                 std::unique_ptr<mojom::blink::SharedWorkerClient> client,
                 mojom::blink::SharedWorkerClientRequest request);

  // Unique identifier for the parent document of a worker (unique within a
  // given process).
  using DocumentID = unsigned long long;
  static DocumentID GetId(Document*);

  service_manager::InterfaceProvider* interface_provider_;
  mojom::blink::SharedWorkerConnectorPtr connector_;

  using ClientSet = mojo::StrongBindingSet<mojom::blink::SharedWorkerClient>;
  using ClientMap = HashMap<DocumentID, std::unique_ptr<ClientSet>>;
  ClientMap client_map_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerRepositoryClientImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_SHARED_WORKER_REPOSITORY_CLIENT_IMPL_H_
