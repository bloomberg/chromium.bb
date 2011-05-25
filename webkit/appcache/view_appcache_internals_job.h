// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
#define WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_

#include <string>

#include "net/url_request/url_request_simple_job.h"
#include "webkit/appcache/appcache_service.h"

namespace net {
class URLRequest;
}  // namespace net

namespace appcache {

// A job subclass that implements a protocol to inspect the internal
// state of appcache service.
class ViewAppCacheInternalsJob : public net::URLRequestSimpleJob {
 public:
  // Stores handle to appcache service for getting information.
  ViewAppCacheInternalsJob(net::URLRequest* request, AppCacheService* service);

  // Fetches the AppCache Info and calls StartAsync after it is done.
  virtual void Start();

  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

  // Overridden method from net::URLRequestJob.
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

 private:
  virtual ~ViewAppCacheInternalsJob();

  void AppCacheDone(int rv);

  // Adds HTML from appcache information to out.
  void GenerateHTMLAppCacheInfo(std::string* out) const;
  void GetAppCacheInfoAsync();
  void RemoveAppCacheInfoAsync(const std::string& manifest_url_spec);

  // This is a common callback for both remove and getinfo for appcache.
  scoped_refptr<net::CancelableCompletionCallback<ViewAppCacheInternalsJob> >
      appcache_done_callback_;

  scoped_refptr<AppCacheInfoCollection> info_collection_;
  // Not owned.
  AppCacheService* appcache_service_;

  DISALLOW_COPY_AND_ASSIGN(ViewAppCacheInternalsJob);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
