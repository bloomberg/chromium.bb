// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_CORE_H_
#define NET_URL_REQUEST_URL_FETCHER_CORE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class HttpResponseHeaders;
class IOBuffer;
class URLFetcherDelegate;
class URLFetcherFileWriter;
class URLRequestContextGetter;
class URLRequestThrottlerEntryInterface;

class URLFetcherCore
    : public base::RefCountedThreadSafe<URLFetcherCore>,
      public URLRequest::Delegate {
 public:
  URLFetcherCore(URLFetcher* fetcher,
                 const GURL& original_url,
                 URLFetcher::RequestType request_type,
                 URLFetcherDelegate* d);

  // Starts the load.  It's important that this not happen in the constructor
  // because it causes the IO thread to begin AddRef()ing and Release()ing
  // us.  If our caller hasn't had time to fully construct us and take a
  // reference, the IO thread could interrupt things, run a task, Release()
  // us, and destroy us, leaving the caller with an already-destroyed object
  // when construction finishes.
  void Start();

  // Stops any in-progress load and ensures no callback will happen.  It is
  // safe to call this multiple times.
  void Stop();

  // URLFetcher-like functions.

  // For POST requests, set |content_type| to the MIME type of the
  // content and set |content| to the data to upload.
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content);
  void SetUploadFilePath(const std::string& upload_content_type,
                         const base::FilePath& file_path,
                         scoped_refptr<base::TaskRunner> file_task_runner);
  void SetChunkedUpload(const std::string& upload_content_type);
  // Adds a block of data to be uploaded in a POST body. This can only be
  // called after Start().
  void AppendChunkToUpload(const std::string& data, bool is_last_chunk);
  // |flags| are flags to apply to the load operation--these should be
  // one or more of the LOAD_* flags defined in net/base/load_flags.h.
  void SetLoadFlags(int load_flags);
  int GetLoadFlags() const;
  void SetReferrer(const std::string& referrer);
  void SetExtraRequestHeaders(const std::string& extra_request_headers);
  void AddExtraRequestHeader(const std::string& header_line);
  void GetExtraRequestHeaders(HttpRequestHeaders* headers) const;
  void SetRequestContext(URLRequestContextGetter* request_context_getter);
  // Set the URL that should be consulted for the third-party cookie
  // blocking policy.
  void SetFirstPartyForCookies(const GURL& first_party_for_cookies);
  // Set the key and data callback that is used when setting the user
  // data on any URLRequest objects this object creates.
  void SetURLRequestUserData(
      const void* key,
      const URLFetcher::CreateDataCallback& create_data_callback);
  void SetStopOnRedirect(bool stop_on_redirect);
  void SetAutomaticallyRetryOn5xx(bool retry);
  void SetMaxRetriesOn5xx(int max_retries);
  int GetMaxRetriesOn5xx() const;
  base::TimeDelta GetBackoffDelay() const;
  void SetAutomaticallyRetryOnNetworkChanges(int max_retries);
  void SaveResponseToFileAtPath(
      const base::FilePath& file_path,
      scoped_refptr<base::TaskRunner> file_task_runner);
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::TaskRunner> file_task_runner);
  HttpResponseHeaders* GetResponseHeaders() const;
  HostPortPair GetSocketAddress() const;
  bool WasFetchedViaProxy() const;
  const GURL& GetOriginalURL() const;
  const GURL& GetURL() const;
  const URLRequestStatus& GetStatus() const;
  int GetResponseCode() const;
  const ResponseCookies& GetCookies() const;
  bool FileErrorOccurred(base::PlatformFileError* out_error_code) const;
  // Reports that the received content was malformed (i.e. failed parsing
  // or validation).  This makes the throttling logic that does exponential
  // back-off when servers are having problems treat the current request as
  // a failure.  Your call to this method will be ignored if your request is
  // already considered a failure based on the HTTP response code or response
  // headers.
  void ReceivedContentWasMalformed();
  bool GetResponseAsString(std::string* out_response_string) const;
  bool GetResponseAsFilePath(bool take_ownership,
                             base::FilePath* out_response_path);

  // Overridden from URLRequest::Delegate:
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(URLRequest* request,
                               int bytes_read) OVERRIDE;
  virtual void OnCertificateRequested(
      URLRequest* request,
      SSLCertRequestInfo* cert_request_info) OVERRIDE;

  URLFetcherDelegate* delegate() const { return delegate_; }
  static void CancelAll();
  static int GetNumFetcherCores();
  static void SetEnableInterceptionForTests(bool enabled);
  static void SetIgnoreCertificateRequests(bool ignored);

 private:
  friend class base::RefCountedThreadSafe<URLFetcherCore>;

  // How should the response be stored?
  enum ResponseDestinationType {
    STRING,  // Default: In a std::string
    PERMANENT_FILE,  // Write to a permanent file.
    TEMP_FILE,  // Write to a temporary file.
  };

  class Registry {
   public:
    Registry();
    ~Registry();

    void AddURLFetcherCore(URLFetcherCore* core);
    void RemoveURLFetcherCore(URLFetcherCore* core);

    void CancelAll();

    int size() const {
      return fetchers_.size();
    }

   private:
    std::set<URLFetcherCore*> fetchers_;

    DISALLOW_COPY_AND_ASSIGN(Registry);
  };

  virtual ~URLFetcherCore();

  // Wrapper functions that allow us to ensure actions happen on the right
  // thread.
  void StartOnIOThread();
  void StartURLRequest();
  void DidCreateFile(int result);
  void StartURLRequestWhenAppropriate();
  void CancelURLRequest();
  void OnCompletedURLRequest(base::TimeDelta backoff_delay);
  void InformDelegateFetchIsComplete();
  void NotifyMalformedContent();
  void DidCloseFile(int result);
  void RetryOrCompleteUrlFetch();

  // Deletes the request, removes it from the registry, and removes the
  // destruction observer.
  void ReleaseRequest();

  // Returns the max value of exponential back-off release time for
  // |original_url_| and |url_|.
  base::TimeTicks GetBackoffReleaseTime();

  void CompleteAddingUploadDataChunk(const std::string& data,
                                     bool is_last_chunk);

  // Store the response bytes in |buffer_| in the container indicated by
  // |response_destination_|. Return true if the write has been
  // done, and another read can overwrite |buffer_|.  If this function
  // returns false, it will post a task that will read more bytes once the
  // write is complete.
  bool WriteBuffer(int num_bytes);

  // Handles the result of WriteBuffer.
  void DidWriteBuffer(int result);

  // Read response bytes from the request.
  void ReadResponse();

  // Drop ownership of any file managed by |file_path_|.
  void DisownFile();

  // Notify Delegate about the progress of upload/download.
  void InformDelegateUploadProgress();
  void InformDelegateUploadProgressInDelegateThread(int64 current, int64 total);
  void InformDelegateDownloadProgress();
  void InformDelegateDownloadProgressInDelegateThread(int64 current,
                                                      int64 total);
  void InformDelegateDownloadDataIfNecessary(int bytes_read);
  void InformDelegateDownloadDataInDelegateThread(
      scoped_ptr<std::string> download_data);

  URLFetcher* fetcher_;              // Corresponding fetcher object
  GURL original_url_;                // The URL we were asked to fetch
  GURL url_;                         // The URL we eventually wound up at
  URLFetcher::RequestType request_type_;  // What type of request is this?
  URLRequestStatus status_;          // Status of the request
  URLFetcherDelegate* delegate_;     // Object to notify on completion
  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
                                     // Task runner for the creating thread.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
                                     // Task runner for download file access.
  scoped_refptr<base::TaskRunner> file_task_runner_;
                                     // Task runner for upload file access.
  scoped_refptr<base::TaskRunner> upload_file_task_runner_;
                                     // Task runner for the thread
                                     // on which file access happens.
  scoped_ptr<URLRequest> request_;   // The actual request this wraps
  int load_flags_;                   // Flags for the load operation
  int response_code_;                // HTTP status code for the request
  std::string data_;                 // Results of the request, when we are
                                     // storing the response as a string.
  scoped_refptr<IOBuffer> buffer_;
                                     // Read buffer
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
                                     // Cookie/cache info for the request
  GURL first_party_for_cookies_;     // The first party URL for the request
  // The user data to add to each newly-created URLRequest.
  const void* url_request_data_key_;
  URLFetcher::CreateDataCallback url_request_create_data_callback_;
  ResponseCookies cookies_;          // Response cookies
  HttpRequestHeaders extra_request_headers_;
  scoped_refptr<HttpResponseHeaders> response_headers_;
  bool was_fetched_via_proxy_;
  HostPortPair socket_address_;

  bool upload_content_set_;          // SetUploadData has been called
  std::string upload_content_;       // HTTP POST payload
  base::FilePath upload_file_path_;  // Path to file containing POST payload
  std::string upload_content_type_;  // MIME type of POST payload
  std::string referrer_;             // HTTP Referer header value
  bool is_chunked_upload_;           // True if using chunked transfer encoding

  // Used to determine how long to wait before making a request or doing a
  // retry.
  //
  // Both of them can only be accessed on the IO thread.
  //
  // We need not only the throttler entry for |original_URL|, but also
  // the one for |url|. For example, consider the case that URL A
  // redirects to URL B, for which the server returns a 500
  // response. In this case, the exponential back-off release time of
  // URL A won't increase. If we retry without considering the
  // back-off constraint of URL B, we may send out too many requests
  // for URL A in a short period of time.
  //
  // Both of these will be NULL if
  // URLRequestContext::throttler_manager() is NULL.
  scoped_refptr<URLRequestThrottlerEntryInterface>
      original_url_throttler_entry_;
  scoped_refptr<URLRequestThrottlerEntryInterface> url_throttler_entry_;

  // True if the URLFetcher has been cancelled.
  bool was_cancelled_;

  // If writing results to a file, |file_writer_| will manage creation,
  // writing, and destruction of that file.
  scoped_ptr<URLFetcherFileWriter> file_writer_;

  // Where should responses be saved?
  ResponseDestinationType response_destination_;

  // Path to the file where the response is written.
  base::FilePath response_destination_file_path_;

  // By default any server-initiated redirects are automatically followed.  If
  // this flag is set to true, however, a redirect will halt the fetch and call
  // back to to the delegate immediately.
  bool stop_on_redirect_;
  // True when we're actually stopped due to a redirect halted by the above.  We
  // use this to ensure that |url_| is set to the redirect destination rather
  // than the originally-fetched URL.
  bool stopped_on_redirect_;

  // If |automatically_retry_on_5xx_| is false, 5xx responses will be
  // propagated to the observer, if it is true URLFetcher will automatically
  // re-execute the request, after the back-off delay has expired.
  // true by default.
  bool automatically_retry_on_5xx_;
  // |num_retries_on_5xx_| indicates how many times we've failed to successfully
  // fetch this URL due to 5xx responses.  Once this value exceeds the maximum
  // number of retries specified by the owner URLFetcher instance,
  // we'll give up.
  int num_retries_on_5xx_;
  // Maximum retries allowed when 5xx responses are received.
  int max_retries_on_5xx_;
  // Back-off time delay. 0 by default.
  base::TimeDelta backoff_delay_;

  // The number of retries that have been attempted due to ERR_NETWORK_CHANGED.
  int num_retries_on_network_changes_;
  // Maximum retries allowed when the request fails with ERR_NETWORK_CHANGED.
  // 0 by default.
  int max_retries_on_network_changes_;

  // Timer to poll the progress of uploading for POST and PUT requests.
  // When crbug.com/119629 is fixed, scoped_ptr is not necessary here.
  scoped_ptr<base::RepeatingTimer<URLFetcherCore> >
      upload_progress_checker_timer_;
  // Number of bytes sent so far.
  int64 current_upload_bytes_;
  // Number of bytes received so far.
  int64 current_response_bytes_;
  // Total expected bytes to receive (-1 if it cannot be determined).
  int64 total_response_bytes_;

  // TODO(willchan): Get rid of this after debugging crbug.com/90971.
  base::debug::StackTrace stack_trace_;

  static base::LazyInstance<Registry> g_registry;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherCore);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_CORE_H_
