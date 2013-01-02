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
// If URLRequestFilter::Factory can't find a handle for the request, it passes
// it through to URLRequestInetJob::Factory and lets the default network stack
// handle it.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILTER_H_
#define NET_URL_REQUEST_URL_REQUEST_FILTER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;

namespace net {
class URLRequestJob;

class NET_EXPORT URLRequestFilter {
 public:
  // scheme,hostname -> ProtocolHandler
  typedef std::map<std::pair<std::string, std::string>,
      URLRequestJobFactory::ProtocolHandler* > HostnameHandlerMap;
  // URL -> ProtocolHandler
  typedef base::hash_map<std::string, URLRequestJobFactory::ProtocolHandler*>
      UrlHandlerMap;

  ~URLRequestFilter();

  static URLRequest::ProtocolFactory Factory;

  // Singleton instance for use.
  static URLRequestFilter* GetInstance();

  void AddHostnameHandler(const std::string& scheme,
                          const std::string& hostname,
                          URLRequest::ProtocolFactory* factory);
  void AddHostnameProtocolHandler(
      const std::string& scheme,
      const std::string& hostname,
      scoped_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler);
  void RemoveHostnameHandler(const std::string& scheme,
                             const std::string& hostname);

  // Returns true if we successfully added the URL handler.  This will replace
  // old handlers for the URL if one existed.
  bool AddUrlHandler(const GURL& url,
                     URLRequest::ProtocolFactory* factory);
  bool AddUrlProtocolHandler(
      const GURL& url,
      scoped_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler);

  void RemoveUrlHandler(const GURL& url);

  // Clear all the existing URL handlers and unregister with the
  // ProtocolFactory.  Resets the hit count.
  void ClearHandlers();

  // Returns the number of times a handler was used to service a request.
  int hit_count() const { return hit_count_; }

 protected:
  URLRequestFilter();

  // Helper method that looks up the request in the url_handler_map_.
  URLRequestJob* FindRequestHandler(URLRequest* request,
                                    NetworkDelegate* network_delegate,
                                    const std::string& scheme);

  // Maps hostnames to factories.  Hostnames take priority over URLs.
  HostnameHandlerMap hostname_handler_map_;

  // Maps URLs to factories.
  UrlHandlerMap url_handler_map_;

  int hit_count_;

 private:
  // Singleton instance.
  static URLRequestFilter* shared_instance_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFilter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILTER_H_
