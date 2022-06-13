// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;
struct ServiceWorkerVersionBaseInfo;

// ServiceWorkerHost is the host of a service worker execution context in the
// renderer process. One ServiceWorkerHost instance hosts one service worker
// execution context instance.
//
// Lives on the UI thread.
class CONTENT_EXPORT ServiceWorkerHost {
 public:
  ServiceWorkerHost(mojo::PendingAssociatedReceiver<
                        blink::mojom::ServiceWorkerContainerHost> host_receiver,
                    ServiceWorkerVersion* version,
                    base::WeakPtr<ServiceWorkerContextCore> context);

  ServiceWorkerHost(const ServiceWorkerHost&) = delete;
  ServiceWorkerHost& operator=(const ServiceWorkerHost&) = delete;

  ~ServiceWorkerHost();

  int worker_process_id() const { return worker_process_id_; }
  ServiceWorkerVersion* version() const { return version_; }

  // Completes initialization of this provider host. It is called once a
  // renderer process has been found to host the worker.
  void CompleteStartWorkerPreparation(
      int process_id,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver);

  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);
  // Used only when EagerCacheStorageSetupForServiceWorkers is disabled.
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  content::ServiceWorkerContainerHost* container_host() {
    return container_host_.get();
  }

  net::NetworkIsolationKey GetNetworkIsolationKey() const;
  const base::UnguessableToken& GetReportingSource() const;

  StoragePartition* GetStoragePartition() const;

  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);

  void CreateBroadcastChannelProvider(
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);

  base::WeakPtr<ServiceWorkerHost> GetWeakPtr();

  void ReportNoBinderForInterface(const std::string& error);

 private:
  int worker_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // The service worker being hosted. Raw pointer is safe because the version
  // owns |this|.
  const raw_ptr<ServiceWorkerVersion> version_;

  // BrowserInterfaceBroker implementation through which this
  // ServiceWorkerHost exposes worker-scoped Mojo services to the corresponding
  // service worker in the renderer.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `ServiceWorkerHost*` parameter.
  BrowserInterfaceBrokerImpl<ServiceWorkerHost,
                             const ServiceWorkerVersionBaseInfo&>
      broker_;
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  std::unique_ptr<ServiceWorkerContainerHost> container_host_;

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  std::unique_ptr<CodeCacheHostImpl::ReceiverSet> code_cache_host_receivers_;

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver_;

  base::WeakPtrFactory<ServiceWorkerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
