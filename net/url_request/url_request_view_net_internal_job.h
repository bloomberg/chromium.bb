// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNAL_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNAL_JOB_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"

// A job subclass that implements the view-net-internal: protocol, which simply
// provides a debug view of the various network components.
class URLRequestViewNetInternalJob : public URLRequestSimpleJob {
 public:
  URLRequestViewNetInternalJob(URLRequest* request)
      : URLRequestSimpleJob(request) {}

  static URLRequest::ProtocolFactory Factory;

  // override from URLRequestSimpleJob
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNAL_JOB_H_
