// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/sync/engine_impl/loopback_server/loopback_connection_manager.h"

#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "net/http/http_status_code.h"

namespace syncer {

LoopbackConnectionManager::LoopbackConnectionManager(
    CancelationSignal* signal,
    const base::FilePath& persistent_file)
    : ServerConnectionManager("localhost", 0, false, signal),
      loopback_server_(persistent_file) {}

LoopbackConnectionManager::~LoopbackConnectionManager() {}

bool LoopbackConnectionManager::PostBufferToPath(
    PostBufferParams* params,
    const std::string& path,
    const std::string& auth_token) {
  params->response.http_status_code =
      loopback_server_.HandleCommand(params->buffer_in, &params->buffer_out);
  DCHECK_GE(params->response.http_status_code, 0);

  if (params->response.http_status_code != net::HTTP_OK) {
    params->response.server_status = HttpResponse::SYNC_SERVER_ERROR;
    return false;
  }

  params->response.server_status = HttpResponse::SERVER_CONNECTION_OK;
  return true;
}

}  // namespace syncer
