// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A URLRequestJob class that pulls the net and http headers from disk.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_file_job.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedWorkerPool;
}

namespace net {
class URLRequestInterceptor;
}

namespace net {

class URLRequestMockHTTPJob : public URLRequestFileJob {
 public:
  // Note that all file IO is done using |worker_pool|.
  URLRequestMockHTTPJob(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const base::FilePath& file_path,
                        const scoped_refptr<base::TaskRunner>& task_runner);

  virtual void Start() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual void GetResponseInfo(HttpResponseInfo* info) OVERRIDE;
  virtual bool IsRedirectResponse(GURL* location,
                                  int* http_status_code) OVERRIDE;

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler(
      const base::FilePath& base_path,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

  // Respond to all HTTP requests of |hostname| with contents of the file
  // located at |file_path|.
  static void AddHostnameToFileHandler(
      const std::string& hostname,
      const base::FilePath& file,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL.
  static GURL GetMockUrl(const base::FilePath& path);

  // Returns a URLRequestJobFactory::ProtocolHandler that serves
  // URLRequestMockHTTPJob's responding like an HTTP server. |base_path| is the
  // file path leading to the root of the directory to use as the root of the
  // HTTP server.
  static scoped_ptr<URLRequestInterceptor> CreateInterceptor(
      const base::FilePath& base_path,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

  // Returns a URLRequestJobFactory::ProtocolHandler that serves
  // URLRequestMockHTTPJob's responding like an HTTP server. It responds to all
  // requests with the contents of |file|.
  static scoped_ptr<URLRequestInterceptor> CreateInterceptorForSingleFile(
      const base::FilePath& file,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

 protected:
  virtual ~URLRequestMockHTTPJob();

 private:
  void GetResponseInfoConst(HttpResponseInfo* info) const;
  void GetRawHeaders(std::string raw_headers);
  std::string raw_headers_;
  const scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<URLRequestMockHTTPJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMockHTTPJob);
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_
