// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_network_url_loader_factory.h"

#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

FakeNetworkURLLoaderFactory::FakeNetworkURLLoaderFactory() = default;

FakeNetworkURLLoaderFactory::~FakeNetworkURLLoaderFactory() = default;

void FakeNetworkURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  bool is_js = base::EndsWith(url_request.url.path(), ".js",
                              base::CompareCase::INSENSITIVE_ASCII);
  std::string headers = "HTTP/1.1 200 OK\n";
  if (is_js)
    headers += "Content-Type: application/javascript\n";
  headers += "\n";
  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  client->OnReceiveResponse(response);

  std::string body = "this body came from the network";
  if (is_js)
    body = "/*" + body + "*/";
  uint32_t bytes_written = body.size();
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle));
  producer_handle->WriteData(body.data(), &bytes_written,
                             MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  client->OnStartLoadingResponseBody(std::move(consumer_handle));

  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;
  client->OnComplete(status);
}

void FakeNetworkURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace content
