// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_filter.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"

namespace net {

namespace {

class URLRequestFilterProtocolHandler
    : public URLRequestJobFactory::ProtocolHandler {
 public:
  explicit URLRequestFilterProtocolHandler(URLRequest::ProtocolFactory* factory)
      : factory_(factory) {}
  virtual ~URLRequestFilterProtocolHandler() {}

  // URLRequestJobFactory::ProtocolHandler implementation
  virtual URLRequestJob* MaybeCreateJob(
      URLRequest* request, NetworkDelegate* network_delegate) const OVERRIDE {
    return factory_(request, network_delegate, request->url().scheme());
  }

 private:
  URLRequest::ProtocolFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFilterProtocolHandler);
};

}  // namespace

URLRequestFilter* URLRequestFilter::shared_instance_ = NULL;

URLRequestFilter::~URLRequestFilter() {}

// static
URLRequestJob* URLRequestFilter::Factory(URLRequest* request,
                                         NetworkDelegate* network_delegate,
                                         const std::string& scheme) {
  // Returning null here just means that the built-in handler will be used.
  return GetInstance()->FindRequestHandler(request, network_delegate, scheme);
}

// static
URLRequestFilter* URLRequestFilter::GetInstance() {
  if (!shared_instance_)
    shared_instance_ = new URLRequestFilter;
  return shared_instance_;
}

void URLRequestFilter::AddHostnameHandler(const std::string& scheme,
    const std::string& hostname, URLRequest::ProtocolFactory* factory) {
  AddHostnameProtocolHandler(
      scheme, hostname,
      scoped_ptr<URLRequestJobFactory::ProtocolHandler>(
          new URLRequestFilterProtocolHandler(factory)));
}

void URLRequestFilter::AddHostnameProtocolHandler(
    const std::string& scheme,
    const std::string& hostname,
    scoped_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler) {
  DCHECK_EQ(0u, hostname_handler_map_.count(make_pair(scheme, hostname)));
  hostname_handler_map_[make_pair(scheme, hostname)] =
      protocol_handler.release();

  // Register with the ProtocolFactory.
  URLRequest::Deprecated::RegisterProtocolFactory(
      scheme, &URLRequestFilter::Factory);

#ifndef NDEBUG
  // Check to see if we're masking URLs in the url_handler_map_.
  for (UrlHandlerMap::const_iterator i = url_handler_map_.begin();
       i != url_handler_map_.end(); ++i) {
    const GURL& url = GURL(i->first);
    HostnameHandlerMap::iterator host_it =
        hostname_handler_map_.find(make_pair(url.scheme(), url.host()));
    if (host_it != hostname_handler_map_.end())
      NOTREACHED();
  }
#endif  // !NDEBUG
}

void URLRequestFilter::RemoveHostnameHandler(const std::string& scheme,
                                             const std::string& hostname) {
  HostnameHandlerMap::iterator iter =
      hostname_handler_map_.find(make_pair(scheme, hostname));
  DCHECK(iter != hostname_handler_map_.end());

  delete iter->second;
  hostname_handler_map_.erase(iter);
  // Note that we don't unregister from the URLRequest ProtocolFactory as
  // this would left no protocol factory for the scheme.
  // URLRequestFilter::Factory will keep forwarding the requests to the
  // URLRequestInetJob.
}

bool URLRequestFilter::AddUrlHandler(
    const GURL& url,
    URLRequest::ProtocolFactory* factory) {
  return AddUrlProtocolHandler(
      url,
      scoped_ptr<URLRequestJobFactory::ProtocolHandler>(
          new URLRequestFilterProtocolHandler(factory)));
}


bool URLRequestFilter::AddUrlProtocolHandler(
    const GURL& url,
    scoped_ptr<URLRequestJobFactory::ProtocolHandler> protocol_handler) {
  if (!url.is_valid())
    return false;
  DCHECK_EQ(0u, url_handler_map_.count(url.spec()));
  url_handler_map_[url.spec()] = protocol_handler.release();

  // Register with the ProtocolFactory.
  URLRequest::Deprecated::RegisterProtocolFactory(url.scheme(),
                                                  &URLRequestFilter::Factory);
  // Check to see if this URL is masked by a hostname handler.
  DCHECK_EQ(0u, hostname_handler_map_.count(make_pair(url.scheme(),
                                                      url.host())));

  return true;
}

void URLRequestFilter::RemoveUrlHandler(const GURL& url) {
  UrlHandlerMap::iterator iter = url_handler_map_.find(url.spec());
  DCHECK(iter != url_handler_map_.end());

  delete iter->second;
  url_handler_map_.erase(iter);
  // Note that we don't unregister from the URLRequest ProtocolFactory as
  // this would left no protocol factory for the scheme.
  // URLRequestFilter::Factory will keep forwarding the requests to the
  // URLRequestInetJob.
}

void URLRequestFilter::ClearHandlers() {
  // Unregister with the ProtocolFactory.
  std::set<std::string> schemes;
  for (UrlHandlerMap::const_iterator i = url_handler_map_.begin();
       i != url_handler_map_.end(); ++i) {
    schemes.insert(GURL(i->first).scheme());
  }
  for (HostnameHandlerMap::const_iterator i = hostname_handler_map_.begin();
       i != hostname_handler_map_.end(); ++i) {
    schemes.insert(i->first.first);
  }
  for (std::set<std::string>::const_iterator scheme = schemes.begin();
       scheme != schemes.end(); ++scheme) {
    URLRequest::Deprecated::RegisterProtocolFactory(*scheme, NULL);
  }

  STLDeleteValues(&url_handler_map_);
  STLDeleteValues(&hostname_handler_map_);
  hit_count_ = 0;
}

URLRequestFilter::URLRequestFilter() : hit_count_(0) { }

URLRequestJob* URLRequestFilter::FindRequestHandler(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const std::string& scheme) {
  URLRequestJob* job = NULL;
  if (request->url().is_valid()) {
    // Check the hostname map first.
    const std::string& hostname = request->url().host();

    HostnameHandlerMap::iterator i =
        hostname_handler_map_.find(make_pair(scheme, hostname));
    if (i != hostname_handler_map_.end())
      job = i->second->MaybeCreateJob(request, network_delegate);

    if (!job) {
      // Not in the hostname map, check the url map.
      const std::string& url = request->url().spec();
      UrlHandlerMap::iterator i = url_handler_map_.find(url);
      if (i != url_handler_map_.end())
        job = i->second->MaybeCreateJob(request, network_delegate);
    }
  }
  if (job) {
    DVLOG(1) << "URLRequestFilter hit for " << request->url().spec();
    hit_count_++;
  }
  return job;
}

}  // namespace net
