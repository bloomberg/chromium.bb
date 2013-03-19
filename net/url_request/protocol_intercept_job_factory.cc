// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/protocol_intercept_job_factory.h"

#include "base/logging.h"

namespace net {

ProtocolInterceptJobFactory::ProtocolInterceptJobFactory(
    scoped_ptr<URLRequestJobFactory> job_factory,
    scoped_ptr<ProtocolHandler> protocol_handler)
    : job_factory_(job_factory.Pass()),
      protocol_handler_(protocol_handler.Pass()) {
}

ProtocolInterceptJobFactory::~ProtocolInterceptJobFactory() {}

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

bool ProtocolInterceptJobFactory::IsHandledProtocol(
    const std::string& scheme) const {
  return job_factory_->IsHandledProtocol(scheme);
}

bool ProtocolInterceptJobFactory::IsHandledURL(const GURL& url) const {
  return job_factory_->IsHandledURL(url);
}

}  // namespace net
