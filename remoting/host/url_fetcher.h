// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_FETCHER_H_
#define REMOTING_HOST_URL_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

class GURL;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}  // namespace net

namespace remoting {

// UrlFetcher implements HTTP querying functionality that is used in
// remoting code (e.g. in GaiaOAuthClient). It takes care of switching
// threads when neccessary and provides interface that is simpler to
// use than net::UrlRequest.
//
// This code is a simplified version of content::UrlFetcher from
// content/common/net. It implements only features that remoting code
// needs.
class UrlFetcher {
 public:
  enum Method {
    GET,
    POST,
  };

  typedef base::Callback<void(const net::URLRequestStatus& status,
                              int response_code,
                              const std::string& response)>
      DoneCallback;

  UrlFetcher(const GURL& url, Method method);
  ~UrlFetcher();

  void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter);
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content);
  void SetHeader(const std::string& key, const std::string& value);
  void Start(const DoneCallback& done_callback);

 private:
  // Ref-counted core of the implementation.
  class Core;

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(UrlFetcher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FETCHER_H_
