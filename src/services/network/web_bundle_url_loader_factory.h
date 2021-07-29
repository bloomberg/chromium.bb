// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class WebBundleMemoryQuotaConsumer;

class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleURLLoaderFactory {
 public:
  // Used for UMA. Append-only.
  enum class SubresourceWebBundleLoadResult {
    kSuccess = 0,
    kMetadataParseError = 1,
    kMemoryQuotaExceeded = 2,
    kServingConstraintsNotMet = 3,
    kWebBundleFetchFailed = 4,
    kMaxValue = kWebBundleFetchFailed,
  };

  WebBundleURLLoaderFactory(
      const GURL& bundle_url,
      mojo::Remote<mojom::WebBundleHandle> web_bundle_handle,
      std::unique_ptr<WebBundleMemoryQuotaConsumer>
          web_bundle_memory_quota_consumer,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      absl::optional<std::string> devtools_request_id);
  ~WebBundleURLLoaderFactory();
  WebBundleURLLoaderFactory(const WebBundleURLLoaderFactory&) = delete;
  WebBundleURLLoaderFactory& operator=(const WebBundleURLLoaderFactory&) =
      delete;

  base::WeakPtr<WebBundleURLLoaderFactory> GetWeakPtr() const;

  void SetBundleStream(mojo::ScopedDataPipeConsumerHandle body);
  void ReportErrorAndCancelPendingLoaders(SubresourceWebBundleLoadResult result,
                                          mojom::WebBundleErrorType error,
                                          const std::string& message);
  mojo::PendingRemote<mojom::URLLoaderClient> WrapURLLoaderClient(
      mojo::PendingRemote<mojom::URLLoaderClient> wrapped);

  void StartSubresourceRequest(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client,
      base::Time request_start_time,
      base::TimeTicks request_start_time_ticks);

  void OnWebBundleFetchFailed();

 private:
  class BundleDataSource;
  class URLLoader;

  bool HasError() const;

  void OnBeforeSendHeadersComplete(
      base::WeakPtr<URLLoader> loader,
      int result,
      const absl::optional<net::HttpRequestHeaders>& headers);
  void QueueOrStartLoader(base::WeakPtr<URLLoader> loader);

  void StartLoad(base::WeakPtr<URLLoader> loader);
  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(base::WeakPtr<URLLoader> loader,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);
  void OnHeadersReceivedComplete(
      base::WeakPtr<URLLoader> loader,
      const std::string& original_header,
      uint64_t payload_offset,
      uint64_t payload_length,
      int result,
      const absl::optional<std::string>& headers,
      const absl::optional<GURL>& preserve_fragment_on_redirect_url);
  void SendResponseToLoader(base::WeakPtr<URLLoader> loader,
                            const std::string& headers,
                            uint64_t payload_offset,
                            uint64_t payload_length);

  void OnMemoryQuotaExceeded();
  void OnDataCompleted();
  void MaybeReportLoadResult(SubresourceWebBundleLoadResult result);

  GURL bundle_url_;
  mojo::Remote<mojom::WebBundleHandle> web_bundle_handle_;
  const absl::optional<::url::Origin> request_initiator_origin_lock_;
  std::unique_ptr<WebBundleMemoryQuotaConsumer>
      web_bundle_memory_quota_consumer_;
  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;
  absl::optional<std::string> devtools_request_id_;
  std::unique_ptr<BundleDataSource> source_;
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  web_package::mojom::BundleMetadataPtr metadata_;
  absl::optional<SubresourceWebBundleLoadResult> load_result_;
  bool data_completed_ = false;
  std::vector<base::WeakPtr<URLLoader>> pending_loaders_;
  base::WeakPtrFactory<WebBundleURLLoaderFactory> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_URL_LOADER_FACTORY_H_
