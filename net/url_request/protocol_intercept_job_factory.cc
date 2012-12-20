// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/protocol_intercept_job_factory.h"

#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job_manager.h"

class GURL;

namespace net {

ProtocolInterceptJobFactory::ProtocolInterceptJobFactory(
    scoped_ptr<URLRequestJobFactory> job_factory,
    scoped_ptr<ProtocolHandler> protocol_handler)
    : job_factory_(job_factory.Pass()),
      protocol_handler_(protocol_handler.Pass()) {
}

ProtocolInterceptJobFactory::~ProtocolInterceptJobFactory() {}

bool ProtocolInterceptJobFactory::SetProtocolHandler(
    const std::string& scheme, ProtocolHandler* protocol_handler) {
  return job_factory_->SetProtocolHandler(scheme, protocol_handler);
}

void ProtocolInterceptJobFactory::AddInterceptor(Interceptor* interceptor) {
  return job_factory_->AddInterceptor(interceptor);
}

URLRequestJob* ProtocolInterceptJobFactory::MaybeCreateJobWithInterceptor(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeCreateJobWithInterceptor(request, network_delegate);
}

URLRequestJob* ProtocolInterceptJobFactory::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK(CalledOnValidThread());
  URLRequestJob* job = protocol_handler_->MaybeCreateJob(request,
                                                         network_delegate);
  if (job)
    return job;
  return job_factory_->MaybeCreateJobWithProtocolHandler(
      scheme, request, network_delegate);
}

URLRequestJob* ProtocolInterceptJobFactory::MaybeInterceptRedirect(
    const GURL& location,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeInterceptRedirect(
      location, request, network_delegate);
}

URLRequestJob* ProtocolInterceptJobFactory::MaybeInterceptResponse(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  return job_factory_->MaybeInterceptResponse(request, network_delegate);
}

bool ProtocolInterceptJobFactory::IsHandledProtocol(
    const std::string& scheme) const {
  return job_factory_->IsHandledProtocol(scheme);
}

bool ProtocolInterceptJobFactory::IsHandledURL(const GURL& url) const {
  return job_factory_->IsHandledURL(url);
}

}  // namespace net
