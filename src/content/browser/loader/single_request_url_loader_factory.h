// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

// An implementation of SharedURLLoaderFactory which handles only a single
// request. It's an error to call CreateLoaderAndStart() more than a total of
// one time across this object or any of its clones.
class CONTENT_EXPORT SingleRequestURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader>,
      mojo::PendingRemote<network::mojom::URLLoaderClient>)>;

  explicit SingleRequestURLLoaderFactory(RequestHandler handler);

  // SharedURLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

 private:
  class PendingFactory;
  class HandlerState;

  explicit SingleRequestURLLoaderFactory(scoped_refptr<HandlerState> state);
  ~SingleRequestURLLoaderFactory() override;

  scoped_refptr<HandlerState> state_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory,
                    scoped_refptr<SingleRequestURLLoaderFactory>>
      receivers_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_
