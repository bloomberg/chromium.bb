// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_
#define CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/service_worker/dispatch_fetch_event_params.mojom.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "url/gurl.h"

namespace content {

// The URLLoaderClient for receiving a navigation preload response. It reports
// the response back to ServiceWorkerContextClient.
//
// This class lives on the service worker thread and is owned by
// ServiceWorkerContextClient.
class NavigationPreloadRequest final : public network::mojom::URLLoaderClient {
 public:
  // |owner| must outlive |this|.
  NavigationPreloadRequest(
      base::WeakPtr<ServiceWorkerContextClient> owner,
      int fetch_event_id,
      const GURL& url,
      blink::mojom::FetchEventPreloadHandlePtr preload_handle);
  ~NavigationPreloadRequest() override;

  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  void MaybeReportResponseToOwner();
  void ReportErrorToOwner(const std::string& message,
                          const std::string& unsanitized_message);

  // TODO(crbug.com/907311): This is just a WeakPtr to do a CHECK that the owner
  // really outlives this, as we've been getting related crashes. Change this to
  // a raw pointer when the bug is fixed.
  base::WeakPtr<ServiceWorkerContextClient> owner_;

  const int fetch_event_id_;
  const GURL url_;
  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> binding_;

  std::unique_ptr<blink::WebURLResponse> response_;
  mojo::ScopedDataPipeConsumerHandle body_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_NAVIGATION_PRELOAD_REQUEST_H_
