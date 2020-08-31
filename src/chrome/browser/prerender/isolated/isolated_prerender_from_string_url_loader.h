// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_FROM_STRING_URL_LOADER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_FROM_STRING_URL_LOADER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace mojo {
class SimpleWatcher;
}

namespace net {
class StringIOBuffer;
}

class IsolatedPrerenderFromStringURLLoader : public network::mojom::URLLoader {
 public:
  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

  IsolatedPrerenderFromStringURLLoader(
      std::unique_ptr<PrefetchedMainframeResponseContainer> response,
      const network::ResourceRequest& tentative_resource_request);

  ~IsolatedPrerenderFromStringURLLoader() override;

  // Called when the response should be served to the user. Returns a handler.
  RequestHandler ServingResponseHandler();

 private:
  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Binds |this| to the mojo handlers and starts the network request using
  // |request|. After this method is called, |this| manages its own lifetime.
  void BindAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Called when the mojo handle's state changes, either by being ready for more
  // data or an error.
  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state);

  // Finishes the request with the given net error.
  void Finish(int error);

  // Sends data on the mojo pipe.
  void TransferRawData();

  // Unbinds and deletes |this|.
  void OnMojoDisconnect();

  // Deletes |this| if it is not bound to the mojo pipes.
  void MaybeDeleteSelf();

  // The response that will be sent to mojo.
  network::mojom::URLResponseHeadPtr head_;
  scoped_refptr<net::StringIOBuffer> body_buffer_;

  // Keeps track of the position of the data transfer.
  int write_position_ = 0;

  // The length of |body_buffer_|.
  const int bytes_of_raw_data_to_transfer_ = 0;

  // Mojo plumbing.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  base::WeakPtrFactory<IsolatedPrerenderFromStringURLLoader> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(IsolatedPrerenderFromStringURLLoader);
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_FROM_STRING_URL_LOADER_H_
