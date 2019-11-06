// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_URL_LOADER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class BundledExchangesReader;

// A class to implements network::mojom::URLLoaderFactory that supports
// BundledExchanges.
class CONTENT_EXPORT BundledExchangesURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  explicit BundledExchangesURLLoaderFactory(
      std::unique_ptr<BundledExchangesReader> reader);
  ~BundledExchangesURLLoaderFactory() override;

  // Set a |network::mojom::URLLoaderFactory| remote interface used for requests
  // that are not found in the BundledExchanges. This will override the existing
  // fallback_factory if it was set previously.
  void SetFallbackFactory(
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader_request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr loader_client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

 private:
  class EntryLoader;
  friend class EntryLoader;

  BundledExchangesReader* GetReader() { return reader_.get(); }

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  std::unique_ptr<BundledExchangesReader> reader_;
  mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory_;

  base::WeakPtrFactory<BundledExchangesURLLoaderFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_URL_LOADER_FACTORY_H_
