// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// URLLoaderFactory for handling about: URLs. This treats everything as
// about:blank since no other about: features should be available to web
// content.
class AboutURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  AboutURLLoaderFactory();
  ~AboutURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest loader) override;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(AboutURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ABOUT_URL_LOADER_FACTORY_H_
