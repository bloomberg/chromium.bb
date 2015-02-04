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
  enum FailurePhase {
    START = 0,
    READ_ASYNC = 1,
    READ_SYNC = 2,
    MAX_FAILURE_PHASE = 3,
  };

  // Note that all file IO is done using |worker_pool|.
  URLRequestMockHTTPJob(URLRequest* request,
                        NetworkDelegate* network_delegate,
                        const base::FilePath& file_path,
                        const scoped_refptr<base::TaskRunner>& task_runner);

  void Start() override;
  bool ReadRawData(IOBuffer* buf, int buf_size, int* bytes_read) override;
  bool GetMimeType(std::string* mime_type) const override;
  int GetResponseCode() const override;
  bool GetCharset(std::string* charset) override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  bool IsRedirectResponse(GURL* location, int* http_status_code) override;

  // Adds the testing URLs to the URLRequestFilter, both under HTTP and HTTPS.
  static void AddUrlHandlers(
      const base::FilePath& base_path,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL.
  static GURL GetMockUrl(const base::FilePath& path);
  static GURL GetMockHttpsUrl(const base::FilePath& path);

  // Given the path to a file relative to the path passed to AddUrlHandler(),
  // construct a mock URL that reports |net_error| at given |phase| of the
  // request. Reporting |net_error| ERR_IO_PENDING results in a hung request.
  static GURL GetMockUrlWithFailure(const base::FilePath& path,
                                    FailurePhase phase,
                                    int net_error);

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
  ~URLRequestMockHTTPJob() override;

 private:
  void GetResponseInfoConst(HttpResponseInfo* info) const;
  void SetHeadersAndStart(const std::string& raw_headers);
  // Checks query part of request url, and reports an error if it matches.
  // Error is parsed out from the query and is reported synchronously.
  // Reporting ERR_IO_PENDING results in a hung request.
  // The "readasync" error is posted asynchronously.
  bool MaybeReportErrorOnPhase(FailurePhase phase);

  std::string raw_headers_;
  const scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<URLRequestMockHTTPJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMockHTTPJob);
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_MOCK_HTTP_JOB_H_
