// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_
#define WEBKIT_APPCACHE_APPCACHE_URL_REQUEST_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_export.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

// A net::URLRequestJob derivative that knows how to return a response stored
// in the appcache.
class APPCACHE_EXPORT AppCacheURLRequestJob : public net::URLRequestJob,
                                              public AppCacheStorage::Delegate {
 public:
  AppCacheURLRequestJob(net::URLRequest* request, AppCacheStorage* storage);
  virtual ~AppCacheURLRequestJob();

  // Informs the job of what response it should deliver. Only one of these
  // methods should be called, and only once per job. A job will sit idle and
  // wait indefinitely until one of the deliver methods is called.
  void DeliverAppCachedResponse(const GURL& manifest_url, int64 group_id,
                                int64 cache_id, const AppCacheEntry& entry,
                                bool is_fallback);
  void DeliverNetworkResponse();
  void DeliverErrorResponse();

  bool is_waiting() const {
    return delivery_type_ == AWAITING_DELIVERY_ORDERS;
  }

  bool is_delivering_appcache_response() const {
    return delivery_type_ == APPCACHED_DELIVERY;
  }

  bool is_delivering_network_response() const {
    return delivery_type_ == NETWORK_DELIVERY;
  }

  bool is_delivering_error_response() const {
    return delivery_type_ == ERROR_DELIVERY;
  }

  // Accessors for the info about the appcached response, if any,
  // that this job has been instructed to deliver. These are only
  // valid to call if is_delivering_appcache_response.
  const GURL& manifest_url() const { return manifest_url_; }
  int64 group_id() const { return group_id_; }
  int64 cache_id() const { return cache_id_; }
  const AppCacheEntry& entry() const { return entry_; }

  // net::URLRequestJob's Kill method is made public so the users of this
  // class in the appcache namespace can call it.
  virtual void Kill() OVERRIDE;

  // Returns true if the job has been started by the net library.
  bool has_been_started() const {
    return has_been_started_;
  }

  // Returns true if the job has been killed.
  bool has_been_killed() const {
    return has_been_killed_;
  }

  // Returns true if the cache entry was not found in the disk cache.
  bool cache_entry_not_found() const {
    return cache_entry_not_found_;
  }

 private:
  friend class AppCacheRequestHandlerTest;
  friend class AppCacheURLRequestJobTest;

  enum DeliveryType {
    AWAITING_DELIVERY_ORDERS,
    APPCACHED_DELIVERY,
    NETWORK_DELIVERY,
    ERROR_DELIVERY
  };

  // Returns true if one of the Deliver methods has been called.
  bool has_delivery_orders() const {
    return !is_waiting();
  }

  void MaybeBeginDelivery();
  void BeginDelivery();

  // AppCacheStorage::Delegate methods
  virtual void OnResponseInfoLoaded(
      AppCacheResponseInfo* response_info, int64 response_id) OVERRIDE;

  const net::HttpResponseInfo* http_info() const;
  bool is_range_request() const { return range_requested_.IsValid(); }
  void SetupRangeResponse();

  // AppCacheResponseReader completion callback
  void OnReadComplete(int result);

  // net::URLRequestJob methods, see url_request_job.h for doc comments
  virtual void Start() OVERRIDE;
  virtual net::LoadState GetLoadState() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;

  // Sets extra request headers for Job types that support request headers.
  // This is how we get informed of range-requests.
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;

  // FilterContext methods
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;

  AppCacheStorage* storage_;
  bool has_been_started_;
  bool has_been_killed_;
  DeliveryType delivery_type_;
  GURL manifest_url_;
  int64 group_id_;
  int64 cache_id_;
  AppCacheEntry entry_;
  bool is_fallback_;
  bool cache_entry_not_found_;
  scoped_refptr<AppCacheResponseInfo> info_;
  net::HttpByteRange range_requested_;
  scoped_ptr<net::HttpResponseInfo> range_response_info_;
  scoped_ptr<AppCacheResponseReader> reader_;
  net::OldCompletionCallbackImpl<AppCacheURLRequestJob> read_callback_;
  base::WeakPtrFactory<AppCacheURLRequestJob> weak_factory_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_REQUEST_HANDLER_H_
