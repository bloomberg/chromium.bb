// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"

#include "base/logging.h"
#include "services/network/public/cpp/resource_request.h"

namespace network {

TestURLLoaderFactory::Response::Response() = default;
TestURLLoaderFactory::Response::~Response() = default;
TestURLLoaderFactory::Response::Response(const Response&) = default;

TestURLLoaderFactory::Pending::Pending() = default;
TestURLLoaderFactory::Pending::~Pending() = default;

TestURLLoaderFactory::Pending::Pending(Pending&& other) = default;
TestURLLoaderFactory::Pending& TestURLLoaderFactory::Pending::operator=(
    Pending&& other) = default;

TestURLLoaderFactory::TestURLLoaderFactory() {}

TestURLLoaderFactory::~TestURLLoaderFactory() {}

void TestURLLoaderFactory::AddResponse(const GURL& url,
                                       const ResourceResponseHead& head,
                                       const std::string& content,
                                       const URLLoaderCompletionStatus& status,
                                       const Redirects& redirects) {
  Response response;
  response.url = url;
  response.redirects = redirects;
  response.head = head;
  response.content = content;
  response.status = status;
  responses_[url] = response;

  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (CreateLoaderAndStartInternal(it->url, it->client.get())) {
      pending_.erase(it);
      return;
    }
  }
}

void TestURLLoaderFactory::AddResponse(const std::string& url,
                                       const std::string& content) {
  ResourceResponseHead head;
  head.headers = new net::HttpResponseHeaders(
      "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
  head.mime_type = "text/html";
  URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  AddResponse(GURL(url), head, content, status);
}

void TestURLLoaderFactory::ClearResponses() {
  responses_.clear();
}

void TestURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (CreateLoaderAndStartInternal(url_request.url, client.get()))
    return;

  Pending pending;
  pending.url = url_request.url;
  pending.client = std::move(client);
  pending_.push_back(std::move(pending));
}

void TestURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  NOTIMPLEMENTED();
}

bool TestURLLoaderFactory::CreateLoaderAndStartInternal(
    const GURL& url,
    mojom::URLLoaderClient* client) {
  auto it = responses_.find(url);
  if (it == responses_.end())
    return false;

  CHECK(it->second.redirects.empty()) << "TODO(jam): handle redirects";
  client->OnReceiveResponse(it->second.head, nullptr);
  mojo::DataPipe data_pipe(it->second.content.size());
  uint32_t bytes_written = it->second.content.size();
  CHECK_EQ(MOJO_RESULT_OK, data_pipe.producer_handle->WriteData(
                               it->second.content.data(), &bytes_written,
                               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  client->OnComplete(it->second.status);
  return true;
}

}  // namespace network
