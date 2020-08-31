// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace network {
namespace mojom {
class CrossOriginEmbedderPolicyReporter;
}  // namespace mojom

struct CrossOriginEmbedderPolicy;

}  // namespace network

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerMainResourceHandleCore;

// This class is used to manage the lifetime of ServiceWorkerProviderHosts
// created for main resource requests (navigations and web workers). This is a
// UI thread class, with a pendant class on the core thread, the
// ServiceWorkerMainResourceHandleCore.
//
// The lifetime of the ServiceWorkerMainResourceHandle, the
// ServiceWorkerMainResourceHandleCore and the ServiceWorkerProviderHost are the
// following:
//   1) We create a ServiceWorkerMainResourceHandle on the UI thread without
//   populating the member service worker provider info. This also leads to the
//   creation of a ServiceWorkerMainResourceHandleCore.
//
//   2) When the navigation request is sent to the core thread, we include a
//   pointer to the ServiceWorkerMainResourceHandleCore.
//
//   3) If we pre-create a ServiceWorkerProviderHost for this navigation, it
//   is added to ServiceWorkerContextCore and its provider info is passed to
//   ServiceWorkerMainResourceHandle on the UI thread via
//   ServiceWorkerMainResourceHandleCore. See
//   ServiceWorkerMainResourceHandleCore::OnCreatedProviderHost() and
//   ServiceWorkerMainResourceHandle::OnCreatedProviderHost() for details.
//
//   4) When the navigation is ready to commit, the NavigationRequest will
//   call ServiceWorkerMainResourceHandle::OnBeginNavigationCommit() to
//     - complete the initialization for the ServiceWorkerProviderHost.
//     - take out the provider info to be sent as part of navigation commit IPC.
//
//   5) When the navigation finishes, the ServiceWorkerMainResourceHandle is
//   destroyed. The destructor of the ServiceWorkerMainResourceHandle destroys
//   the provider info which in turn leads to the destruction of an unclaimed
//   ServiceWorkerProviderHost, and posts a task to destroy the
//   ServiceWorkerMainResourceHandleCore on the core thread.
class CONTENT_EXPORT ServiceWorkerMainResourceHandle {
 public:
  ServiceWorkerMainResourceHandle(
      ServiceWorkerContextWrapper* context_wrapper,
      ServiceWorkerAccessedCallback on_service_worker_accessed);
  ~ServiceWorkerMainResourceHandle();

  // Called after a ServiceWorkerProviderHost tied with |provider_info|
  // was pre-created for the navigation.
  void OnCreatedProviderHost(
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info);

  // Called when the navigation is ready to commit.
  // Provides |render_process_id|, |render_frame_id|, and
  // |cross_origin_embedder_policy| to the pre-created provider host. Fills in
  // |out_provider_info| so the caller can send it to the renderer process as
  // part of the navigation commit IPC.
  // |out_provider_info| can be filled as null if we failed to pre-create the
  // provider host for some security reasons.
  void OnBeginNavigationCommit(
      int render_process_id,
      int render_frame_id,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr* out_provider_info);

  // Similar to OnBeginNavigationCommit() for shared workers (and dedicated
  // workers when PlzDedicatedWorker is on).
  // |cross_origin_embedder_policy| is passed to the pre-created provider
  // host.
  void OnBeginWorkerCommit(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy);

  blink::mojom::ServiceWorkerProviderInfoForClientPtr TakeProviderInfo() {
    return std::move(provider_info_);
  }

  bool has_provider_info() const { return !!provider_info_; }

  ServiceWorkerMainResourceHandleCore* core() { return core_; }

  const ServiceWorkerContextWrapper* context_wrapper() const {
    return context_wrapper_.get();
  }

  base::WeakPtr<ServiceWorkerMainResourceHandle> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info_;

  ServiceWorkerMainResourceHandleCore* core_;
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  base::WeakPtrFactory<ServiceWorkerMainResourceHandle> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMainResourceHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_
