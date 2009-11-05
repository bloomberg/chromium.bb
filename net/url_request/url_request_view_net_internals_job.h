// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNALS_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNALS_JOB_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"

// A job subclass that implements a protocol to inspect the internal
// state of the network stack. The exact format of the URLs is left up to
// the caller, and is described by a URLFormat instance passed into
// the constructor.
class URLRequestViewNetInternalsJob : public URLRequestSimpleJob {
 public:
  class URLFormat;

  // |url_format| must remain valid for the duration |this|'s lifespan.
  URLRequestViewNetInternalsJob(URLRequest* request,
                                URLFormat* url_format)
      : URLRequestSimpleJob(request), url_format_(url_format) {}

  // override from URLRequestSimpleJob
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

 private:
  ~URLRequestViewNetInternalsJob() {}

  URLFormat* url_format_;
};

// Describes how to pack/unpack the filter string (details)
// from a URL.
class URLRequestViewNetInternalsJob::URLFormat {
 public:
  virtual ~URLFormat() {}
  virtual std::string GetDetails(const GURL& url) = 0;
  virtual GURL MakeURL(const std::string& details) = 0;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_VIEW_NET_INTERNALS_JOB_H_
