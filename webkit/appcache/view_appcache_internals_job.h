// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
#define WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_

#include <string>

#include "net/url_request/url_request_simple_job.h"
#include "webkit/appcache/appcache_service.h"

class URLRequest;

namespace appcache {

// A job subclass that implements a protocol to inspect the internal
// state of appcache service.
class ViewAppCacheInternalsJob : public URLRequestSimpleJob {
 public:
  // Stores handle to appcache service for getting information.
  ViewAppCacheInternalsJob(URLRequest* request, AppCacheService* service);

  // Fetches the AppCache Info and calls StartAsync after it is done.
  virtual void Start();

  // URLRequestSimpleJob methods:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

  // Callback after asynchronously fetching contents for appcache service.
  void OnGotAppCacheInfo(int rv);

 private:
  ~ViewAppCacheInternalsJob();

  // Adds HTML from appcache information to out.
  void GenerateHTMLAppCacheInfo(std::string* out) const;

  // Callback for post-processing.
  scoped_refptr<net::CancelableCompletionCallback<ViewAppCacheInternalsJob> >
      appcache_info_callback_;
  // Store appcache information.
  scoped_refptr<AppCacheInfoCollection> info_collection_;
  // Not owned.
  AppCacheService* appcache_service_;

  DISALLOW_COPY_AND_ASSIGN(ViewAppCacheInternalsJob);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_VIEW_APPCACHE_INTERNALS_JOB_H_
