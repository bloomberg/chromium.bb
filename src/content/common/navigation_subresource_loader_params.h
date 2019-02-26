// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_

#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class ServiceWorkerObjectHost;

// For NetworkService glues:
// Navigation parameters that are necessary to set-up a subresource loader
// for the frame that is going to be created by the navigation.
// Passed from the browser to the renderer when the navigation commits when
// NetworkService or its glue code for relevant features is enabled.
struct CONTENT_EXPORT SubresourceLoaderParams {
  SubresourceLoaderParams();
  ~SubresourceLoaderParams();

  SubresourceLoaderParams(SubresourceLoaderParams&& other);
  SubresourceLoaderParams& operator=(SubresourceLoaderParams&& other);

  // For AppCache.
  // Subresource loader factory info for appcache, that is to be used to
  // create a subresource loader in the renderer.
  network::mojom::URLLoaderFactoryPtrInfo appcache_loader_factory_info;

  // For ServiceWorkers.
  // The controller service worker, non-null if the frame is to be
  // controlled by the service worker.
  //
  // |controller_service_worker_info->object_info| is "incomplete". It must be
  // updated before being sent over Mojo and then registered with
  // |controller_service_worker_object_host|. See
  // ServiceWorkerObjectHost::CreateIncompleteObjectInfo() for details.
  mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info;
  base::WeakPtr<ServiceWorkerObjectHost> controller_service_worker_object_host;
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
