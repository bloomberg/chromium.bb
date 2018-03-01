// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
struct NetworkTrafficAnnotationTag;
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceRequest;
struct ResourceResponseHead;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace network {

class SimpleURLLoaderStreamConsumer;

// Creates and wraps a URLLoader, and runs it to completion. It's recommended
// that consumers use this class instead of URLLoader directly, due to the
// complexity of the API.
//
// Deleting a SimpleURLLoader before it completes cancels the requests and frees
// any resources it is using (including any partially downloaded files).
//
// Each SimpleURLLoader can only be used for a single request.
//
// TODO(mmenke): Support the following:
// * Consumer-provided methods to receive streaming (with backpressure).
// * Uploads (Fixed strings, files, data streams (with backpressure), chunked
// uploads). ResourceRequest may already have some support, but should make it
// simple.
// * Maybe some sort of retry backoff or delay?  ServiceURLLoaderContext enables
// throttling for its URLFetchers.  Could additionally/alternatively support
// 503 + Retry-After.
class COMPONENT_EXPORT(NETWORK_CPP) SimpleURLLoader {
 public:
  // When a failed request should automatically be retried. These are intended
  // to be ORed together.
  enum RetryMode {
    RETRY_NEVER = 0x0,
    // Retries whenever the server returns a 5xx response code.
    RETRY_ON_5XX = 0x1,
    // Retries on net::ERR_NETWORK_CHANGED.
    RETRY_ON_NETWORK_CHANGE = 0x2,
  };

  // The maximum size DownloadToString will accept.
  static const size_t kMaxBoundedStringDownloadSize;

  // Maximum upload body size to send as a block to the URLLoaderFactory. This
  // data may appear in memory twice for a while, in the retry case, and there
  // may briefly be 3 to 5 copies as it's copied over the Mojo pipe:  This
  // class's copy (with retries enabled), the source mojo pipe's input copy, the
  // copy on the IPC buffer, the destination mojo pipe's copy, and the network
  // service's copy.
  //
  // Only exposed for tests.
  static const size_t kMaxUploadStringSizeToCopy;

  // Callback used when downloading the response body as a std::string.
  // |response_body| is the body of the response, or nullptr on failure.
  using BodyAsStringCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;

  // Callback used when download the response body to a file. On failure, |path|
  // will be empty.
  using DownloadToFileCompleteCallback =
      base::OnceCallback<void(const base::FilePath& path)>;

  // Callback used when a redirect is being followed. It is safe to delete the
  // SimpleURLLoader during the callback.
  using OnRedirectCallback =
      base::RepeatingCallback<void(const net::RedirectInfo& redirect_info,
                                   const ResourceResponseHead& response_head)>;

  // Callback used when a redirect is being followed. It is safe to delete the
  // SimpleURLLoader during the callback.
  using OnResponseStartedCallback =
      base::RepeatingCallback<void(const GURL& final_url,
                                   const ResourceResponseHead& response_head)>;

  // Creates a SimpleURLLoader for |resource_request|. The request can be
  // started by calling any one of the Download methods once. The loader may not
  // be reused.
  static std::unique_ptr<SimpleURLLoader> Create(
      std::unique_ptr<ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

  virtual ~SimpleURLLoader();

  // Starts the request using |network_context|. The SimpleURLLoader will
  // accumulate all downloaded data in an in-memory string of bounded size. If
  // |max_body_size| is exceeded, the request will fail with
  // net::ERR_INSUFFICIENT_RESOURCES. |max_body_size| must be no greater than 1
  // MiB. For anything larger, it's recommended to either save to a temp file,
  // or consume the data as it is received.
  //
  // Whether the request succeeds or fails, the URLLoaderFactory pipe is closed,
  // or the body exceeds |max_body_size|, |body_as_string_callback| will be
  // invoked on completion. Deleting the SimpleURLLoader before the callback is
  // invoked will result in cancelling the request, and the callback will not be
  // called.
  virtual void DownloadToString(mojom::URLLoaderFactory* url_loader_factory,
                                BodyAsStringCallback body_as_string_callback,
                                size_t max_body_size) = 0;

  // Same as DownloadToString, but downloads to a buffer of unbounded size,
  // potentially causing a crash if the amount of addressable memory is
  // exceeded. It's recommended consumers use one of the other download methods
  // instead (DownloadToString if the body is expected to be of reasonable
  // length, or DownloadToFile otherwise).
  virtual void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallback body_as_string_callback) = 0;

  // SimpleURLLoader will download the entire response to a file at the
  // specified path. File I/O will happen on another sequence, so it's safe to
  // use this on any sequence.
  //
  // If there's a file, network, Mojo, or http error, or the max limit
  // is exceeded, the file will be automatically destroyed before the callback
  // is invoked and en empty path passed to the callback, unless
  // SetAllowPartialResults() and/or SetAllowHttpErrorResults() were used to
  // indicate partial results are allowed.
  //
  // If the SimpleURLLoader is destroyed before it has invoked the callback, the
  // downloaded file will be deleted asynchronously and the callback will not be
  // invoked, regardless of other settings.
  virtual void DownloadToFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

  // Same as DownloadToFile, but creates a temporary file instead of taking a
  // FilePath.
  virtual void DownloadToTempFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      int64_t max_body_size = std::numeric_limits<int64_t>::max()) = 0;

  // SimpleURLLoader will stream the response body to
  // SimpleURLLoaderStreamConsumer on the current thread. Destroying the
  // SimpleURLLoader will cancel the request, and prevent any subsequent
  // methods from being invoked on the Handler. The SimpleURLLoader may also be
  // destroyed in any of the Handler's callbacks.
  //
  // |stream_handler| must remain valid until either the SimpleURLLoader is
  // deleted, or the handler's OnComplete() method has been invoked by the
  // SimpleURLLoader.
  virtual void DownloadAsStream(
      mojom::URLLoaderFactory* url_loader_factory,
      SimpleURLLoaderStreamConsumer* stream_consumer) = 0;

  // Sets callback to be invoked during redirects. Callback may delete the
  // SimpleURLLoader.
  virtual void SetOnRedirectCallback(
      const OnRedirectCallback& on_redirect_callback) = 0;

  // Sets callback to be invoked when the response has started. May be called
  // multiple times if retries are enabled.
  // Callback may delete the SimpleURLLoader.
  virtual void SetOnResponseStartedCallback(
      const OnResponseStartedCallback& on_response_started_callback) = 0;

  // Sets whether partially received results are allowed. Defaults to false.
  // When true, if an error is received after reading the body starts or the max
  // allowed body size exceeded, the partial response body that was received
  // will be provided to the caller. The partial response body may be an empty
  // string.
  //
  // When downloading as a stream, this has no observable effect.
  //
  // May only be called before the request is started.
  virtual void SetAllowPartialResults(bool allow_partial_results) = 0;

  // Sets whether bodies of non-2xx responses are returned. May only be called
  // before the request is started.
  //
  // When false, if a non-2xx result is received (Other than a redirect), the
  // request will fail with net::FAILED without waiting to read the response
  // body, though headers will be accessible through response_info().
  //
  // When true, non-2xx responses are treated no differently than other
  // responses, so their response body is returned just as with any other
  // response code, and when they complete, net_error() will return net::OK, if
  // no other problem occurs.
  //
  // Defaults to false.
  // TODO(mmenke): Consider adding a new error code for this.
  virtual void SetAllowHttpErrorResults(bool allow_http_error_results) = 0;

  // Attaches the specified string as the upload body. Depending on the length
  // of the string, the string may be copied to the URLLoader, or may be
  // streamed to it from the current process. May only be called once, and only
  // if ResourceRequest passed to the constructor had a null |request_body|.
  //
  // |content_type| will overwrite any Content-Type header in the
  // ResourceRequest passed to Create().
  //
  // TODO(mmenke): This currently always requires a copy. Update DataElement not
  // to require this.
  virtual void AttachStringForUpload(
      const std::string& upload_data,
      const std::string& upload_content_type) = 0;

  // Helper method to attach a file for upload, so the consumer won't need to
  // open the file itself off-thread. May only be called once, and only if the
  // ResourceRequest passed to the constructor had a null |request_body|.
  //
  // |content_type| will overwrite any Content-Type header in the
  // ResourceRequest passed to Create().
  virtual void AttachFileForUpload(const base::FilePath& upload_file_path,
                                   const std::string& upload_content_type) = 0;

  // Sets the when to try and the max number of times to retry a request, if
  // any. |max_retries| is the number of times to retry the request, not
  // counting the initial request. |retry_mode| is a combination of one or more
  // RetryModes, indicating when the request should be retried. If it is
  // RETRY_NEVER, |max_retries| must be 0.
  //
  // By default, a request will not be retried.
  //
  // When a request is retried, the the request will start again using the
  // initial content::ResourceRequest, even if the request was redirected.
  //
  // Calling this multiple times will overwrite the values previously passed to
  // this method. May only be called before the request is started.
  //
  // Cannot retry requests with an upload body that contains a data pipe that
  // was added to the ResourceRequest passed to Create() by the consumer.
  virtual void SetRetryOptions(int max_retries, int retry_mode) = 0;

  // Returns the net::Error representing the final status of the request. May
  // only be called once the loader has informed the caller of completion.
  virtual int NetError() const = 0;

  // The ResourceResponseHead for the request. Will be nullptr if ResponseInfo
  // was never received. May only be called once the loader has informed the
  // caller of completion.
  virtual const ResourceResponseHead* ResponseInfo() const = 0;

  // Returns the URL that this loader is processing. May only be called once the
  // loader has informed the caller of completion.
  virtual const GURL& GetFinalURL() const = 0;

 protected:
  SimpleURLLoader();

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoader);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_H_
