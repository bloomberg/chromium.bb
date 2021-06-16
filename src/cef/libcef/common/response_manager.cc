// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/response_manager.h"
#include "libcef/common/cef_messages.h"

#include "base/logging.h"

CefResponseManager::CefResponseManager() : next_request_id_(0) {}

int CefResponseManager::GetNextRequestId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++next_request_id_;
}

int CefResponseManager::RegisterHandler(CefRefPtr<Handler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int request_id = GetNextRequestId();
  TRACE_EVENT_ASYNC_BEGIN1("cef", "CefResponseManager::Handler", request_id,
                           "request_id", request_id);
  handlers_.insert(std::make_pair(request_id, handler));
  return request_id;
}

bool CefResponseManager::RunHandler(const Cef_Response_Params& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(params.request_id, 0);
  HandlerMap::iterator it = handlers_.find(params.request_id);
  if (it != handlers_.end()) {
    TRACE_EVENT0("cef", "CefResponseManager::RunHandler");
    it->second->OnResponse(params);
    handlers_.erase(it);
    TRACE_EVENT_ASYNC_END1("cef", "CefResponseManager::Handler",
                           params.request_id, "success", 1);
    return true;
  }
  TRACE_EVENT_ASYNC_END1("cef", "CefResponseManager::Handler",
                         params.request_id, "success", 0);
  return false;
}

void CefResponseManager::RegisterAckHandler(int request_id,
                                            CefRefPtr<AckHandler> handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ack_handlers_.insert(std::make_pair(request_id, handler));
}

bool CefResponseManager::RunAckHandler(int request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(request_id, 0);
  AckHandlerMap::iterator it = ack_handlers_.find(request_id);
  if (it != ack_handlers_.end()) {
    it->second->OnResponseAck();
    ack_handlers_.erase(it);
    return true;
  }
  return false;
}
