// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/syncapi_server_connection_manager.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"

namespace syncer {

SyncAPIBridgedConnection::SyncAPIBridgedConnection(
    ServerConnectionManager* scm,
    HttpPostProviderFactory* factory)
    : Connection(scm), factory_(factory) {
  post_provider_ = factory_->Create();
}

SyncAPIBridgedConnection::~SyncAPIBridgedConnection() {
  DCHECK(post_provider_);
  factory_->Destroy(post_provider_);
  post_provider_ = NULL;
}

bool SyncAPIBridgedConnection::Init(const char* path,
                                    const std::string& auth_token,
                                    const std::string& payload,
                                    HttpResponse* response) {
  std::string sync_server;
  int sync_server_port = 0;
  bool use_ssl = false;
  GetServerParams(&sync_server, &sync_server_port, &use_ssl);
  std::string connection_url = MakeConnectionURL(sync_server, path, use_ssl);

  HttpPostProviderInterface* http = post_provider_;
  http->SetURL(connection_url.c_str(), sync_server_port);

  if (!auth_token.empty()) {
    std::string headers;
    headers = "Authorization: Bearer " + auth_token;
    http->SetExtraRequestHeaders(headers.c_str());
  }

  // Must be octet-stream, or the payload may be parsed for a cookie.
  http->SetPostPayload("application/octet-stream", payload.length(),
                       payload.data());

  // Issue the POST, blocking until it finishes.
  int error_code = 0;
  int response_code = 0;
  if (!http->MakeSynchronousPost(&error_code, &response_code)) {
    DVLOG(1) << "Http POST failed, error returns: " << error_code;
    response->server_status = HttpResponse::ServerConnectionCodeFromNetError(
        error_code);
    return false;
  }

  // We got a server response, copy over response codes and content.
  response->response_code = response_code;
  response->content_length =
      static_cast<int64>(http->GetResponseContentLength());
  response->payload_length =
      static_cast<int64>(http->GetResponseContentLength());
  if (response->response_code < 400)
    response->server_status = HttpResponse::SERVER_CONNECTION_OK;
  else if (response->response_code == net::HTTP_UNAUTHORIZED)
    response->server_status = HttpResponse::SYNC_AUTH_ERROR;
  else
    response->server_status = HttpResponse::SYNC_SERVER_ERROR;

  // Write the content into our buffer.
  buffer_.assign(http->GetResponseContent(), http->GetResponseContentLength());
  return true;
}

void SyncAPIBridgedConnection::Abort() {
  DCHECK(post_provider_);
  post_provider_->Abort();
}

SyncAPIServerConnectionManager::SyncAPIServerConnectionManager(
    const std::string& server,
    int port,
    bool use_ssl,
    HttpPostProviderFactory* factory,
    CancelationSignal* cancelation_signal)
    : ServerConnectionManager(server,
                              port,
                              use_ssl,
                              cancelation_signal),
      post_provider_factory_(factory) {
  DCHECK(post_provider_factory_.get());
}

SyncAPIServerConnectionManager::~SyncAPIServerConnectionManager() {}

ServerConnectionManager::Connection*
SyncAPIServerConnectionManager::MakeConnection() {
  return new SyncAPIBridgedConnection(this, post_provider_factory_.get());
}

}  // namespace syncer
