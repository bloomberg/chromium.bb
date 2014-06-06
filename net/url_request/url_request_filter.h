// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to help filter URLRequest jobs based on the URL of the request
// rather than just the scheme.  Example usage:
//
// // Use as an "http" handler.
// URLRequest::RegisterProtocolFactory("http", &URLRequestFilter::Factory);
// // Add special handling for the URL http://foo.com/
// URLRequestFilter::GetInstance()->AddUrlHandler(
//     GURL("http://foo.com/"),
//     &URLRequestCustomJob::Factory);
//
// If URLRequestFilter::Factory can't find a handler for the request, it
// returns null to URLRequestJobManager and lets a built-in protocol factory
// handle the request.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILTER_H_
#define NET_URL_REQUEST_URL_REQUEST_FILTER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request.h"

class GURL;

namespace net {
class URLRequestJob;
class URLRequestInterceptor;

class NET_EXPORT URLRequestFilter {
 public:
  ~URLRequestFilter();

  static URLRequest::ProtocolFactory Factory;

  // Singleton instance for use.
  static URLRequestFilter* GetInstance();

  void AddHostnameHandler(const std::string& scheme,
                          const std::string& hostname,
                          URLRequest::ProtocolFactory* factory);
  void AddHostnameInterceptor(
      const std::string& scheme,
      const std::string& hostname,
      scoped_ptr<URLRequestInterceptor> interceptor);
  void RemoveHostnameHandler(const std::string& scheme,
                             const std::string& hostname);

  // Returns true if we successfully added the URL handler.  This will replace
  // old handlers for the URL if one existed.
  bool AddUrlHandler(const GURL& url,
                     URLRequest::ProtocolFactory* factory);
  bool AddUrlInterceptor(const GURL& url,
                         scoped_ptr<URLRequestInterceptor> interceptor);

  void RemoveUrlHandler(const GURL& url);

  // Clear all the existing URL handlers and unregister with the
  // ProtocolFactory.  Resets the hit count.
  void ClearHandlers();

  // Returns the number of times a handler was used to service a request.
  int hit_count() const { return hit_count_; }

 private:
  // scheme,hostname -> URLRequestInterceptor
  typedef std::map<std::pair<std::string, std::string>,
      URLRequestInterceptor* > HostnameInterceptorMap;
  // URL -> URLRequestInterceptor
  typedef base::hash_map<std::string, URLRequestInterceptor*> URLInterceptorMap;

  URLRequestFilter();

  // Helper method that passes the request to any interceptors registered to
  // to handle it.  Returns the resulting URLRequestJob if they create one.
  URLRequestJob* MaybeInterceptRequest(URLRequest* request,
                                       NetworkDelegate* network_delegate,
                                       const std::string& scheme);

  // Maps hostnames to interceptors.  Hostnames take priority over URLs.
  HostnameInterceptorMap hostname_interceptor_map_;

  // Maps URLs to interceptors.
  URLInterceptorMap url_interceptor_map_;

  int hit_count_;

  // Singleton instance.
  static URLRequestFilter* shared_instance_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFilter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILTER_H_
