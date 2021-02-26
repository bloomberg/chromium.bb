// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// A URLLoaderFactory which just fails to create a loader with
// net::ERR_NOT_IMPLEMENTED.
class COMPONENT_EXPORT(NETWORK_CPP) NotImplementedURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  NotImplementedURLLoaderFactory();
  ~NotImplementedURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(NotImplementedURLLoaderFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NOT_IMPLEMENTED_URL_LOADER_FACTORY_H_
