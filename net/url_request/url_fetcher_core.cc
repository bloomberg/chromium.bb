// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_core.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/tracked_objects.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_throttler_manager.h"

namespace {

const int kBufferSize = 4096;
const int kUploadProgressTimerInterval = 100;
bool g_interception_enabled = false;
bool g_ignore_certificate_requests = false;

}  // namespace

namespace net {

// URLFetcherCore::Registry ---------------------------------------------------

URLFetcherCore::Registry::Registry() {}
URLFetcherCore::Registry::~Registry() {}

void URLFetcherCore::Registry::AddURLFetcherCore(URLFetcherCore* core) {
  DCHECK(!ContainsKey(fetchers_, core));
  fetchers_.insert(core);
}

void URLFetcherCore::Registry::RemoveURLFetcherCore(URLFetcherCore* core) {
  DCHECK(ContainsKey(fetchers_, core));
  fetchers_.erase(core);
}

void URLFetcherCore::Registry::CancelAll() {
  while (!fetchers_.empty())
    (*fetchers_.begin())->CancelURLRequest();
}

// URLFetcherCore -------------------------------------------------------------

// static
base::LazyInstance<URLFetcherCore::Registry>
    URLFetcherCore::g_registry = LAZY_INSTANCE_INITIALIZER;

URLFetcherCore::URLFetcherCore(URLFetcher* fetcher,
                               const GURL& original_url,
                               URLFetcher::RequestType request_type,
                               URLFetcherDelegate* d)
    : fetcher_(fetcher),
      original_url_(original_url),
      request_type_(request_type),
      delegate_(d),
      delegate_task_runner_(
          base::ThreadTaskRunnerHandle::Get()),
      request_(NULL),
      load_flags_(LOAD_NORMAL),
      response_code_(URLFetcher::RESPONSE_CODE_INVALID),
      buffer_(new IOBuffer(kBufferSize)),
      url_request_data_key_(NULL),
      was_fetched_via_proxy_(false),
      upload_content_set_(false),
      is_chunked_upload_(false),
      was_cancelled_(false),
      file_writer_(NULL),
      response_destination_(STRING),
      stop_on_redirect_(false),
      stopped_on_redirect_(false),
      automatically_retry_on_5xx_(true),
      num_retries_on_5xx_(0),
      max_retries_on_5xx_(0),
      num_retries_on_network_changes_(0),
      max_retries_on_network_changes_(0),
      current_upload_bytes_(-1),
      current_response_bytes_(0),
      total_response_bytes_(-1) {
  CHECK(original_url_.is_valid());
}

void URLFetcherCore::Start() {
  DCHECK(delegate_task_runner_);
  DCHECK(request_context_getter_) << "We need an URLRequestContext!";
  if (network_task_runner_) {
    DCHECK_EQ(network_task_runner_,
              request_context_getter_->GetNetworkTaskRunner());
  } else {
    network_task_runner_ = request_context_getter_->GetNetworkTaskRunner();
  }
  DCHECK(network_task_runner_.get()) << "We need an IO task runner";

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&URLFetcherCore::StartOnIOThread, this));
}

void URLFetcherCore::Stop() {
  if (delegate_task_runner_)  // May be NULL in tests.
    DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_ = NULL;
  fetcher_ = NULL;
  if (!network_task_runner_.get())
    return;
  if (network_task_runner_->RunsTasksOnCurrentThread()) {
    CancelURLRequest();
  } else {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&URLFetcherCore::CancelURLRequest, this));
  }
}

void URLFetcherCore::SetUploadData(const std::string& upload_content_type,
                                   const std::string& upload_content) {
  DCHECK(!is_chunked_upload_);
  DCHECK(!upload_content_set_);
  DCHECK(upload_content_.empty());
  DCHECK(upload_file_path_.empty());
  DCHECK(upload_content_type_.empty());

  // Empty |upload_content_type| is allowed iff the |upload_content| is empty.
  DCHECK(upload_content.empty() || !upload_content_type.empty());

  upload_content_type_ = upload_content_type;
  upload_content_ = upload_content;
  upload_content_set_ = true;
}

void URLFetcherCore::SetUploadFilePath(
    const std::string& upload_content_type,
    const base::FilePath& file_path,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK(!is_chunked_upload_);
  DCHECK(!upload_content_set_);
  DCHECK(upload_content_.empty());
  DCHECK(upload_file_path_.empty());
  DCHECK(upload_content_type_.empty());
  DCHECK(!upload_content_type.empty());

  upload_content_type_ = upload_content_type;
  upload_file_path_ = file_path;
  upload_file_task_runner_ = file_task_runner;
  upload_content_set_ = true;
}

void URLFetcherCore::SetChunkedUpload(const std::string& content_type) {
  DCHECK(is_chunked_upload_ ||
         (upload_content_type_.empty() &&
          upload_content_.empty()));

  // Empty |content_type| is not allowed here, because it is impossible
  // to ensure non-empty upload content as it is not yet supplied.
  DCHECK(!content_type.empty());

  upload_content_type_ = content_type;
  upload_content_.clear();
  is_chunked_upload_ = true;
}

void URLFetcherCore::AppendChunkToUpload(const std::string& content,
                                         bool is_last_chunk) {
  DCHECK(delegate_task_runner_);
  DCHECK(network_task_runner_.get());
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherCore::CompleteAddingUploadDataChunk, this, content,
                 is_last_chunk));
}

void URLFetcherCore::SetLoadFlags(int load_flags) {
  load_flags_ = load_flags;
}

int URLFetcherCore::GetLoadFlags() const {
  return load_flags_;
}

void URLFetcherCore::SetReferrer(const std::string& referrer) {
  referrer_ = referrer;
}

void URLFetcherCore::SetExtraRequestHeaders(
    const std::string& extra_request_headers) {
  extra_request_headers_.Clear();
  extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void URLFetcherCore::AddExtraRequestHeader(const std::string& header_line) {
  extra_request_headers_.AddHeaderFromString(header_line);
}

void URLFetcherCore::GetExtraRequestHeaders(
    HttpRequestHeaders* headers) const {
  headers->CopyFrom(extra_request_headers_);
}

void URLFetcherCore::SetRequestContext(
    URLRequestContextGetter* request_context_getter) {
  DCHECK(!request_context_getter_);
  DCHECK(request_context_getter);
  request_context_getter_ = request_context_getter;
}

void URLFetcherCore::SetFirstPartyForCookies(
    const GURL& first_party_for_cookies) {
  DCHECK(first_party_for_cookies_.is_empty());
  first_party_for_cookies_ = first_party_for_cookies;
}

void URLFetcherCore::SetURLRequestUserData(
    const void* key,
    const URLFetcher::CreateDataCallback& create_data_callback) {
  DCHECK(key);
  DCHECK(!create_data_callback.is_null());
  url_request_data_key_ = key;
  url_request_create_data_callback_ = create_data_callback;
}

void URLFetcherCore::SetStopOnRedirect(bool stop_on_redirect) {
  stop_on_redirect_ = stop_on_redirect;
}

void URLFetcherCore::SetAutomaticallyRetryOn5xx(bool retry) {
  automatically_retry_on_5xx_ = retry;
}

void URLFetcherCore::SetMaxRetriesOn5xx(int max_retries) {
  max_retries_on_5xx_ = max_retries;
}

int URLFetcherCore::GetMaxRetriesOn5xx() const {
  return max_retries_on_5xx_;
}

base::TimeDelta URLFetcherCore::GetBackoffDelay() const {
  return backoff_delay_;
}

void URLFetcherCore::SetAutomaticallyRetryOnNetworkChanges(int max_retries) {
  max_retries_on_network_changes_ = max_retries;
}

void URLFetcherCore::SaveResponseToFileAtPath(
    const base::FilePath& file_path,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  file_task_runner_ = file_task_runner;
  response_destination_ = URLFetcherCore::PERMANENT_FILE;
  response_destination_file_path_ = file_path;
}

void URLFetcherCore::SaveResponseToTemporaryFile(
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  file_task_runner_ = file_task_runner;
  response_destination_ = URLFetcherCore::TEMP_FILE;
}

HttpResponseHeaders* URLFetcherCore::GetResponseHeaders() const {
  return response_headers_;
}

// TODO(panayiotis): socket_address_ is written in the IO thread,
// if this is accessed in the UI thread, this could result in a race.
// Same for response_headers_ above and was_fetched_via_proxy_ below.
HostPortPair URLFetcherCore::GetSocketAddress() const {
  return socket_address_;
}

bool URLFetcherCore::WasFetchedViaProxy() const {
  return was_fetched_via_proxy_;
}

const GURL& URLFetcherCore::GetOriginalURL() const {
  return original_url_;
}

const GURL& URLFetcherCore::GetURL() const {
  return url_;
}

const URLRequestStatus& URLFetcherCore::GetStatus() const {
  return status_;
}

int URLFetcherCore::GetResponseCode() const {
  return response_code_;
}

const ResponseCookies& URLFetcherCore::GetCookies() const {
  return cookies_;
}

bool URLFetcherCore::FileErrorOccurred(
    base::PlatformFileError* out_error_code) const {

  // Can't have a file error if no file is being created or written to.
  if (!file_writer_)
    return false;

  base::PlatformFileError error_code = file_writer_->error_code();
  if (error_code == base::PLATFORM_FILE_OK)
    return false;

  *out_error_code = error_code;
  return true;
}

void URLFetcherCore::ReceivedContentWasMalformed() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (network_task_runner_.get()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&URLFetcherCore::NotifyMalformedContent, this));
  }
}

bool URLFetcherCore::GetResponseAsString(
    std::string* out_response_string) const {
  if (response_destination_ != URLFetcherCore::STRING)
    return false;

  *out_response_string = data_;
  UMA_HISTOGRAM_MEMORY_KB("UrlFetcher.StringResponseSize",
                          (data_.length() / 1024));

  return true;
}

bool URLFetcherCore::GetResponseAsFilePath(bool take_ownership,
                                           base::FilePath* out_response_path) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  const bool destination_is_file =
      response_destination_ == URLFetcherCore::TEMP_FILE ||
      response_destination_ == URLFetcherCore::PERMANENT_FILE;
  if (!destination_is_file || !file_writer_)
    return false;

  *out_response_path = file_writer_->file_path();

  if (take_ownership) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&URLFetcherCore::DisownFile, this));
  }
  return true;
}

void URLFetcherCore::OnReceivedRedirect(URLRequest* request,
                                        const GURL& new_url,
                                        bool* defer_redirect) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (stop_on_redirect_) {
    stopped_on_redirect_ = true;
    url_ = new_url;
    response_code_ = request_->GetResponseCode();
    was_fetched_via_proxy_ = request_->was_fetched_via_proxy();
    request->Cancel();
    OnReadCompleted(request, 0);
  }
}

void URLFetcherCore::OnResponseStarted(URLRequest* request) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (request_->status().is_success()) {
    response_code_ = request_->GetResponseCode();
    response_headers_ = request_->response_headers();
    socket_address_ = request_->GetSocketAddress();
    was_fetched_via_proxy_ = request_->was_fetched_via_proxy();
    total_response_bytes_ = request_->GetExpectedContentSize();
  }

  ReadResponse();
}

void URLFetcherCore::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (g_ignore_certificate_requests) {
    request->ContinueWithCertificate(NULL);
  } else {
    request->Cancel();
  }
}

void URLFetcherCore::OnReadCompleted(URLRequest* request,
                                     int bytes_read) {
  DCHECK(request == request_);
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!stopped_on_redirect_)
    url_ = request->url();
  URLRequestThrottlerManager* throttler_manager =
      request->context()->throttler_manager();
  if (throttler_manager) {
    url_throttler_entry_ = throttler_manager->RegisterRequestUrl(url_);
  }

  bool waiting_on_write = false;
  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;

    current_response_bytes_ += bytes_read;
    InformDelegateDownloadProgress();
    InformDelegateDownloadDataIfNecessary(bytes_read);

    const int result = response_writer_->Write(
        buffer_, bytes_read,
        base::Bind(&URLFetcherCore::DidWriteBuffer, this));
    if (result < 0) {
      // Write failed or waiting for write completion.
      if (result == ERR_IO_PENDING)
        waiting_on_write = true;
      break;
    }
  } while (request_->Read(buffer_, kBufferSize, &bytes_read));

  const URLRequestStatus status = request_->status();

  if (status.is_success())
    request_->GetResponseCookies(&cookies_);

  // See comments re: HEAD requests in ReadResponse().
  if ((!status.is_io_pending() && !waiting_on_write) ||
      (request_type_ == URLFetcher::HEAD)) {
    status_ = status;
    ReleaseRequest();

    // No more data to write.
    const int result = response_writer_->Finish(
        base::Bind(&URLFetcherCore::DidFinishWriting, this));
    if (result != ERR_IO_PENDING)
      DidFinishWriting(result);
  }
}

void URLFetcherCore::CancelAll() {
  g_registry.Get().CancelAll();
}

int URLFetcherCore::GetNumFetcherCores() {
  return g_registry.Get().size();
}

void URLFetcherCore::SetEnableInterceptionForTests(bool enabled) {
  g_interception_enabled = enabled;
}

void URLFetcherCore::SetIgnoreCertificateRequests(bool ignored) {
  g_ignore_certificate_requests = ignored;
}

URLFetcherCore::~URLFetcherCore() {
  // |request_| should be NULL.  If not, it's unsafe to delete it here since we
  // may not be on the IO thread.
  DCHECK(!request_.get());
}

void URLFetcherCore::StartOnIOThread() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  switch (response_destination_) {
    case STRING:
      response_writer_.reset(new URLFetcherStringWriter(&data_));
      break;

    case PERMANENT_FILE:
    case TEMP_FILE:
      DCHECK(file_task_runner_.get())
          << "Need to set the file task runner.";

      file_writer_ = new URLFetcherFileWriter(file_task_runner_);

      // If the file is successfully created,
      // URLFetcherCore::StartURLRequestWhenAppropriate() will be called.
      if (response_destination_ == PERMANENT_FILE) {
        file_writer_->set_destination_file_path(
            response_destination_file_path_);
      }
      response_writer_.reset(file_writer_);
      break;

    default:
      NOTREACHED();
  }
  DCHECK(response_writer_);
  const int result = response_writer_->Initialize(
      base::Bind(&URLFetcherCore::DidInitializeWriter, this));
  if (result != ERR_IO_PENDING)
    DidInitializeWriter(result);
}

void URLFetcherCore::StartURLRequest() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (was_cancelled_) {
    // Since StartURLRequest() is posted as a *delayed* task, it may
    // run after the URLFetcher was already stopped.
    return;
  }

  DCHECK(request_context_getter_);
  DCHECK(!request_.get());

  g_registry.Get().AddURLFetcherCore(this);
  current_response_bytes_ = 0;
  request_.reset(request_context_getter_->GetURLRequestContext()->CreateRequest(
      original_url_, this));
  request_->set_stack_trace(stack_trace_);
  int flags = request_->load_flags() | load_flags_;
  if (!g_interception_enabled)
    flags = flags | LOAD_DISABLE_INTERCEPT;

  if (is_chunked_upload_)
    request_->EnableChunkedUpload();
  request_->set_load_flags(flags);
  request_->set_referrer(referrer_);
  request_->set_first_party_for_cookies(first_party_for_cookies_.is_empty() ?
      original_url_ : first_party_for_cookies_);
  if (url_request_data_key_ && !url_request_create_data_callback_.is_null()) {
    request_->SetUserData(url_request_data_key_,
                          url_request_create_data_callback_.Run());
  }

  switch (request_type_) {
    case URLFetcher::GET:
      break;

    case URLFetcher::POST:
    case URLFetcher::PUT:
    case URLFetcher::PATCH:
      // Upload content must be set.
      DCHECK(is_chunked_upload_ || upload_content_set_);

      request_->set_method(
          request_type_ == URLFetcher::POST ? "POST" :
          request_type_ == URLFetcher::PUT ? "PUT" : "PATCH");
      extra_request_headers_.SetHeader(HttpRequestHeaders::kContentType,
                                       upload_content_type_);
      if (!upload_content_type_.empty()) {
        extra_request_headers_.SetHeader(HttpRequestHeaders::kContentType,
                                         upload_content_type_);
      }
      if (!upload_content_.empty()) {
        scoped_ptr<UploadElementReader> reader(new UploadBytesElementReader(
            upload_content_.data(), upload_content_.size()));
        request_->set_upload(make_scoped_ptr(
            UploadDataStream::CreateWithReader(reader.Pass(), 0)));
      } else if (!upload_file_path_.empty()) {
        scoped_ptr<UploadElementReader> reader(new UploadFileElementReader(
            upload_file_task_runner_,
            upload_file_path_,
            0, kuint64max, base::Time()));
        request_->set_upload(make_scoped_ptr(
            UploadDataStream::CreateWithReader(reader.Pass(), 0)));
      }

      current_upload_bytes_ = -1;
      // TODO(kinaba): http://crbug.com/118103. Implement upload callback in the
      //  layer and avoid using timer here.
      upload_progress_checker_timer_.reset(
          new base::RepeatingTimer<URLFetcherCore>());
      upload_progress_checker_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUploadProgressTimerInterval),
          this,
          &URLFetcherCore::InformDelegateUploadProgress);
      break;

    case URLFetcher::HEAD:
      request_->set_method("HEAD");
      break;

    case URLFetcher::DELETE_REQUEST:
      request_->set_method("DELETE");
      break;

    default:
      NOTREACHED();
  }

  if (!extra_request_headers_.IsEmpty())
    request_->SetExtraRequestHeaders(extra_request_headers_);

  // There might be data left over from a previous request attempt.
  data_.clear();

  // If we are writing the response to a file, the only caller
  // of this function should have created it and not written yet.
  DCHECK(!file_writer_ || file_writer_->total_bytes_written() == 0);

  request_->Start();
}

void URLFetcherCore::DidInitializeWriter(int result) {
  if (result != OK) {
    delegate_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&URLFetcherCore::InformDelegateFetchIsComplete, this));
    return;
  }
  StartURLRequestWhenAppropriate();
}

void URLFetcherCore::StartURLRequestWhenAppropriate() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (was_cancelled_)
    return;

  DCHECK(request_context_getter_);

  int64 delay = 0LL;
  if (original_url_throttler_entry_ == NULL) {
    URLRequestThrottlerManager* manager =
        request_context_getter_->GetURLRequestContext()->throttler_manager();
    if (manager) {
      original_url_throttler_entry_ =
          manager->RegisterRequestUrl(original_url_);
    }
  }
  if (original_url_throttler_entry_ != NULL) {
    delay = original_url_throttler_entry_->ReserveSendingTimeForNextRequest(
        GetBackoffReleaseTime());
  }

  if (delay == 0) {
    StartURLRequest();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&URLFetcherCore::StartURLRequest, this),
        base::TimeDelta::FromMilliseconds(delay));
  }
}

void URLFetcherCore::CancelURLRequest() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (request_.get()) {
    request_->Cancel();
    ReleaseRequest();
  }
  // Release the reference to the request context. There could be multiple
  // references to URLFetcher::Core at this point so it may take a while to
  // delete the object, but we cannot delay the destruction of the request
  // context.
  request_context_getter_ = NULL;
  first_party_for_cookies_ = GURL();
  url_request_data_key_ = NULL;
  url_request_create_data_callback_.Reset();
  was_cancelled_ = true;
  response_writer_.reset();
  file_writer_ = NULL;
}

void URLFetcherCore::OnCompletedURLRequest(
    base::TimeDelta backoff_delay) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  // Save the status and backoff_delay so that delegates can read it.
  if (delegate_) {
    backoff_delay_ = backoff_delay;
    InformDelegateFetchIsComplete();
  }
}

void URLFetcherCore::InformDelegateFetchIsComplete() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (delegate_)
    delegate_->OnURLFetchComplete(fetcher_);
}

void URLFetcherCore::NotifyMalformedContent() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (url_throttler_entry_ != NULL) {
    int status_code = response_code_;
    if (status_code == URLFetcher::RESPONSE_CODE_INVALID) {
      // The status code will generally be known by the time clients
      // call the |ReceivedContentWasMalformed()| function (which ends up
      // calling the current function) but if it's not, we need to assume
      // the response was successful so that the total failure count
      // used to calculate exponential back-off goes up.
      status_code = 200;
    }
    url_throttler_entry_->ReceivedContentWasMalformed(status_code);
  }
}

void URLFetcherCore::DidFinishWriting(int result) {
  if (result != OK) {
    delegate_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&URLFetcherCore::InformDelegateFetchIsComplete, this));
    return;
  }
  // If the file was successfully closed, then the URL request is complete.
  RetryOrCompleteUrlFetch();
}

void URLFetcherCore::RetryOrCompleteUrlFetch() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  base::TimeDelta backoff_delay;

  // Checks the response from server.
  if (response_code_ >= 500 ||
      status_.error() == ERR_TEMPORARILY_THROTTLED) {
    // When encountering a server error, we will send the request again
    // after backoff time.
    ++num_retries_on_5xx_;

    // Note that backoff_delay may be 0 because (a) the
    // URLRequestThrottlerManager and related code does not
    // necessarily back off on the first error, (b) it only backs off
    // on some of the 5xx status codes, (c) not all URLRequestContexts
    // have a throttler manager.
    base::TimeTicks backoff_release_time = GetBackoffReleaseTime();
    backoff_delay = backoff_release_time - base::TimeTicks::Now();
    if (backoff_delay < base::TimeDelta())
      backoff_delay = base::TimeDelta();

    if (automatically_retry_on_5xx_ &&
        num_retries_on_5xx_ <= max_retries_on_5xx_) {
      StartOnIOThread();
      return;
    }
  } else {
    backoff_delay = base::TimeDelta();
  }

  // Retry if the request failed due to network changes.
  if (status_.error() == ERR_NETWORK_CHANGED &&
      num_retries_on_network_changes_ < max_retries_on_network_changes_) {
    ++num_retries_on_network_changes_;

    // Retry soon, after flushing all the current tasks which may include
    // further network change observers.
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&URLFetcherCore::StartOnIOThread, this));
    return;
  }

  request_context_getter_ = NULL;
  first_party_for_cookies_ = GURL();
  url_request_data_key_ = NULL;
  url_request_create_data_callback_.Reset();
  bool posted = delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherCore::OnCompletedURLRequest, this, backoff_delay));

  // If the delegate message loop does not exist any more, then the delegate
  // should be gone too.
  DCHECK(posted || !delegate_);
}

void URLFetcherCore::ReleaseRequest() {
  upload_progress_checker_timer_.reset();
  request_.reset();
  g_registry.Get().RemoveURLFetcherCore(this);
}

base::TimeTicks URLFetcherCore::GetBackoffReleaseTime() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (original_url_throttler_entry_) {
    base::TimeTicks original_url_backoff =
        original_url_throttler_entry_->GetExponentialBackoffReleaseTime();
    base::TimeTicks destination_url_backoff;
    if (url_throttler_entry_ != NULL &&
        original_url_throttler_entry_ != url_throttler_entry_) {
      destination_url_backoff =
          url_throttler_entry_->GetExponentialBackoffReleaseTime();
    }

    return original_url_backoff > destination_url_backoff ?
        original_url_backoff : destination_url_backoff;
  } else {
    return base::TimeTicks();
  }
}

void URLFetcherCore::CompleteAddingUploadDataChunk(
    const std::string& content, bool is_last_chunk) {
  if (was_cancelled_) {
    // Since CompleteAddingUploadDataChunk() is posted as a *delayed* task, it
    // may run after the URLFetcher was already stopped.
    return;
  }
  DCHECK(is_chunked_upload_);
  DCHECK(request_.get());
  DCHECK(!content.empty());
  request_->AppendChunkToUpload(content.data(),
                                static_cast<int>(content.length()),
                                is_last_chunk);
}

void URLFetcherCore::DidWriteBuffer(int result) {
  if (result < 0) {
    delegate_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&URLFetcherCore::InformDelegateFetchIsComplete, this));
    return;
  }
  // Finished writing buffer_. Read some more.
  ReadResponse();
}

void URLFetcherCore::ReadResponse() {
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  int bytes_read = 0;
  if (request_->status().is_success() &&
      (request_type_ != URLFetcher::HEAD))
    request_->Read(buffer_, kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

void URLFetcherCore::DisownFile() {
  DCHECK(file_writer_);
  file_writer_->DisownFile();
}

void URLFetcherCore::InformDelegateUploadProgress() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (request_.get()) {
    int64 current = request_->GetUploadProgress().position();
    if (current_upload_bytes_ != current) {
      current_upload_bytes_ = current;
      int64 total = -1;
      if (!is_chunked_upload_) {
        total = static_cast<int64>(request_->GetUploadProgress().size());
        // Total may be zero if the UploadDataStream::Init has not been called
        // yet.  Don't send the upload progress until the size is initialized.
        if (!total)
          return;
      }
      delegate_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &URLFetcherCore::InformDelegateUploadProgressInDelegateThread,
              this, current, total));
    }
  }
}

void URLFetcherCore::InformDelegateUploadProgressInDelegateThread(
    int64 current, int64 total) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (delegate_)
    delegate_->OnURLFetchUploadProgress(fetcher_, current, total);
}

void URLFetcherCore::InformDelegateDownloadProgress() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &URLFetcherCore::InformDelegateDownloadProgressInDelegateThread,
          this, current_response_bytes_, total_response_bytes_));
}

void URLFetcherCore::InformDelegateDownloadProgressInDelegateThread(
    int64 current, int64 total) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (delegate_)
    delegate_->OnURLFetchDownloadProgress(fetcher_, current, total);
}

void URLFetcherCore::InformDelegateDownloadDataIfNecessary(int bytes_read) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  if (delegate_ && delegate_->ShouldSendDownloadData()) {
    scoped_ptr<std::string> download_data(
        new std::string(buffer_->data(), bytes_read));
    delegate_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &URLFetcherCore::InformDelegateDownloadDataInDelegateThread,
            this, base::Passed(&download_data)));
  }
}

void URLFetcherCore::InformDelegateDownloadDataInDelegateThread(
    scoped_ptr<std::string> download_data) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  if (delegate_)
    delegate_->OnURLFetchDownloadData(fetcher_, download_data.Pass());
}

}  // namespace net
