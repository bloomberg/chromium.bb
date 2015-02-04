// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_mock_http_job.h"

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

namespace {

const char kMockHostname[] = "mock.http";
const base::FilePath::CharType kMockHeaderFileSuffix[] =
    FILE_PATH_LITERAL(".mock-http-headers");

// String names of failure phases matching FailurePhase enum.
const char* kFailurePhase[] {
  "start",      // START
  "readasync",  // READ_ASYNC
  "readsync",   // READ_SYNC
};

class MockJobInterceptor : public net::URLRequestInterceptor {
 public:
  // When |map_all_requests_to_base_path| is true, all request should return the
  // contents of the file at |base_path|. When |map_all_requests_to_base_path|
  // is false, |base_path| is the file path leading to the root of the directory
  // to use as the root of the HTTP server.
  MockJobInterceptor(
      const base::FilePath& base_path,
      bool map_all_requests_to_base_path,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool)
      : base_path_(base_path),
        map_all_requests_to_base_path_(map_all_requests_to_base_path),
        worker_pool_(worker_pool) {}
  ~MockJobInterceptor() override {}

  // net::URLRequestJobFactory::ProtocolHandler implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestMockHTTPJob(
        request,
        network_delegate,
        map_all_requests_to_base_path_ ? base_path_ : GetOnDiskPath(request),
        worker_pool_->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  }

 private:
  base::FilePath GetOnDiskPath(net::URLRequest* request) const {
    // Conceptually we just want to "return base_path_ + request->url().path()".
    // But path in the request URL is in URL space (i.e. %-encoded spaces).
    // So first we convert base FilePath to a URL, then append the URL
    // path to that, and convert the final URL back to a FilePath.
    GURL file_url(net::FilePathToFileURL(base_path_));
    std::string url = file_url.spec() + request->url().path();
    base::FilePath file_path;
    net::FileURLToFilePath(GURL(url), &file_path);
    return file_path;
  }

  const base::FilePath base_path_;
  const bool map_all_requests_to_base_path_;
  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

std::string DoFileIO(const base::FilePath& file_path) {
  base::FilePath header_file =
      base::FilePath(file_path.value() + kMockHeaderFileSuffix);

  if (!base::PathExists(header_file)) {
    // If there is no mock-http-headers file, fake a 200 OK.
    return "HTTP/1.0 200 OK\n";
  }

  std::string raw_headers;
  base::ReadFileToString(header_file, &raw_headers);
  return raw_headers;
}

// For a given file |path| and |scheme|, return the URL served by the
// URlRequestMockHTTPJob.
GURL GetMockUrlForScheme(const base::FilePath& path,
                         const std::string& scheme) {
  std::string url = scheme + "://" + kMockHostname + "/";
  std::string path_str = path.MaybeAsASCII();
  DCHECK(!path_str.empty());  // We only expect ASCII paths in tests.
  url.append(path_str);
  return GURL(url);
}

}  // namespace

// static
void URLRequestMockHTTPJob::AddUrlHandlers(
    const base::FilePath& base_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool) {
  // Add kMockHostname to net::URLRequestFilter, for both HTTP and HTTPS.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "http", kMockHostname, CreateInterceptor(base_path, worker_pool));
  filter->AddHostnameInterceptor("https", kMockHostname,
                                 CreateInterceptor(base_path, worker_pool));
}

// static
GURL URLRequestMockHTTPJob::GetMockUrl(const base::FilePath& path) {
  return GetMockUrlForScheme(path, "http");
}

// static
GURL URLRequestMockHTTPJob::GetMockHttpsUrl(const base::FilePath& path) {
  return GetMockUrlForScheme(path, "https");
}

// static
GURL URLRequestMockHTTPJob::GetMockUrlWithFailure(const base::FilePath& path,
                                                  FailurePhase phase,
                                                  int net_error) {
  static_assert(arraysize(kFailurePhase) == MAX_FAILURE_PHASE,
                "kFailurePhase must match FailurePhase enum");
  DCHECK_GE(phase, START);
  DCHECK_LE(phase, READ_SYNC);
  std::string url(GetMockUrl(path).spec());
  url.append("?");
  url.append(kFailurePhase[phase]);
  url.append("=");
  url.append(base::IntToString(net_error));
  return GURL(url);
}

// static
scoped_ptr<net::URLRequestInterceptor> URLRequestMockHTTPJob::CreateInterceptor(
    const base::FilePath& base_path,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool) {
  return scoped_ptr<net::URLRequestInterceptor>(
      new MockJobInterceptor(base_path, false, worker_pool));
}

// static
scoped_ptr<net::URLRequestInterceptor>
URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
    const base::FilePath& file,
    const scoped_refptr<base::SequencedWorkerPool>& worker_pool) {
  return scoped_ptr<net::URLRequestInterceptor>(
      new MockJobInterceptor(file, true, worker_pool));
}

URLRequestMockHTTPJob::URLRequestMockHTTPJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& file_path,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : net::URLRequestFileJob(request, network_delegate, file_path, task_runner),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
}

URLRequestMockHTTPJob::~URLRequestMockHTTPJob() {
}

// Public virtual version.
void URLRequestMockHTTPJob::GetResponseInfo(net::HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

bool URLRequestMockHTTPJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  // Override the net::URLRequestFileJob implementation to invoke the default
  // one based on HttpResponseInfo.
  return net::URLRequestJob::IsRedirectResponse(location, http_status_code);
}

// Public virtual version.
void URLRequestMockHTTPJob::Start() {
  if (MaybeReportErrorOnPhase(START))
    return;
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&DoFileIO, file_path_),
      base::Bind(&URLRequestMockHTTPJob::SetHeadersAndStart,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Public virtual version.
bool URLRequestMockHTTPJob::ReadRawData(IOBuffer* buf,
                                        int buf_size,
                                        int* bytes_read) {
  if (MaybeReportErrorOnPhase(READ_SYNC))
    return false;
  if (MaybeReportErrorOnPhase(READ_ASYNC))
    return false;
  return URLRequestFileJob::ReadRawData(buf, buf_size, bytes_read);
}

void URLRequestMockHTTPJob::SetHeadersAndStart(const std::string& raw_headers) {
  if (MaybeReportErrorOnPhase(START))
    return;
  raw_headers_ = raw_headers;
  // Handle CRLF line-endings.
  ReplaceSubstringsAfterOffset(&raw_headers_, 0, "\r\n", "\n");
  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers_, 0, "\n", std::string("\0", 1));
  URLRequestFileJob::Start();
}

bool URLRequestMockHTTPJob::MaybeReportErrorOnPhase(
    FailurePhase current_phase) {
  DCHECK_GE(current_phase, START);
  DCHECK_LE(current_phase, READ_SYNC);
  std::string phase_key(kFailurePhase[current_phase]);
  std::string phase_error_string;
  if (!GetValueForKeyInQuery(request_->url(), phase_key, &phase_error_string))
    return false;

  int net_error;
  if (!base::StringToInt(phase_error_string, &net_error))
    return false;

  if (net_error != net::ERR_IO_PENDING &&
      (current_phase == START || current_phase == READ_SYNC)) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, net_error));
    return true;
  }

  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));

  if (current_phase != READ_ASYNC)
    return true;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &URLRequestMockHTTPJob::NotifyDone,
          weak_ptr_factory_.GetWeakPtr(),
          net::URLRequestStatus(net::URLRequestStatus::FAILED, net_error)));

  return true;
}

// Private const version.
void URLRequestMockHTTPJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  info->headers = new net::HttpResponseHeaders(raw_headers_);
}

bool URLRequestMockHTTPJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers.get() && info.headers->GetMimeType(mime_type);
}

int URLRequestMockHTTPJob::GetResponseCode() const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  // If we have headers, get the response code from them.
  if (info.headers.get())
    return info.headers->response_code();
  return net::URLRequestJob::GetResponseCode();
}

bool URLRequestMockHTTPJob::GetCharset(std::string* charset) {
  net::HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers.get() && info.headers->GetCharset(charset);
}

}  // namespace net
