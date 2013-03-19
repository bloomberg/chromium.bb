// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job_manager.h"

namespace net {

URLRequestJobFactoryImpl::URLRequestJobFactoryImpl() {}

URLRequestJobFactoryImpl::~URLRequestJobFactoryImpl() {
  STLDeleteValues(&protocol_handler_map_);
}

bool URLRequestJobFactoryImpl::SetProtocolHandler(
    const std::string& scheme,
    ProtocolHandler* protocol_handler) {
  DCHECK(CalledOnValidThread());

  if (!protocol_handler) {
    ProtocolHandlerMap::iterator it = protocol_handler_map_.find(scheme);
    if (it == protocol_handler_map_.end())
      return false;

    delete it->second;
    protocol_handler_map_.erase(it);
    return true;
  }

  if (ContainsKey(protocol_handler_map_, scheme))
    return false;
  protocol_handler_map_[scheme] = protocol_handler;
  return true;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  ProtocolHandlerMap::const_iterator it = protocol_handler_map_.find(scheme);
  if (it == protocol_handler_map_.end())
    return NULL;
  return it->second->MaybeCreateJob(request, network_delegate);
}

bool URLRequestJobFactoryImpl::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK(CalledOnValidThread());
  return ContainsKey(protocol_handler_map_, scheme) ||
      URLRequestJobManager::GetInstance()->SupportsScheme(scheme);
}

bool URLRequestJobFactoryImpl::IsHandledURL(const GURL& url) const {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }
  return IsHandledProtocol(url.scheme());
}

}  // namespace net
