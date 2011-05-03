// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_BUFFERED_RESOURCE_LOADER_H_
#define WEBKIT_GLUE_MEDIA_BUFFERED_RESOURCE_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "media/base/seekable_buffer.h"
#include "net/base/file_stream.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "webkit/glue/media/web_data_source.h"
#include "webkit/glue/webmediaplayer_impl.h"

namespace webkit_glue {

const int64 kPositionNotSpecified = -1;

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kDataScheme[] = "data";

// This class works inside demuxer thread and render thread. It contains a
// WebURLLoader and does the actual resource loading. This object does
// buffering internally, it defers the resource loading if buffer is full
// and un-defers the resource loading if it is under buffered.
class BufferedResourceLoader :
    public base::RefCountedThreadSafe<BufferedResourceLoader>,
    public WebKit::WebURLLoaderClient {
 public:
  // kNeverDefer - Aggresively buffer; never defer loading while paused.
  // kReadThenDefer - Request only enough data to fulfill read requests.
  // kThresholdDefer - Try to keep amount of buffered data at a threshold.
  enum DeferStrategy {
    kNeverDefer,
    kReadThenDefer,
    kThresholdDefer,
  };

  typedef Callback0::Type NetworkEventCallback;

  // |url| - URL for the resource to be loaded.
  // |first_byte_position| - First byte to start loading from,
  // |kPositionNotSpecified| for not specified.
  // |last_byte_position| - Last byte to be loaded,
  // |kPositionNotSpecified| for not specified.
  BufferedResourceLoader(const GURL& url,
                         int64 first_byte_position,
                         int64 last_byte_position);

  // Start the resource loading with the specified URL and range.
  // This method operates in asynchronous mode. Once there's a response from the
  // server, success or fail |callback| is called with the result.
  // |callback| is called with the following values:
  // - net::OK
  //   The request has started successfully.
  // - net::ERR_FAILED
  //   The request has failed because of an error with the network.
  // - net::ERR_INVALID_RESPONSE
  //   An invalid response is received from the server.
  // - (Anything else)
  //   An error code that indicates the request has failed.
  // |event_callback| is called when the response is completed, data is
  // received, the request is suspended or resumed.
  virtual void Start(net::CompletionCallback* callback,
                     NetworkEventCallback* event_callback,
                     WebKit::WebFrame* frame);

  // Stop this loader, cancels and request and release internal buffer.
  virtual void Stop();

  // Reads the specified |read_size| from |position| into |buffer| and when
  // the operation is done invoke |callback| with number of bytes read or an
  // error code.
  // |callback| is called with the following values:
  // - (Anything greater than or equal 0)
  //   Read was successful with the indicated number of bytes read.
  // - net::ERR_FAILED
  //   The read has failed because of an error with the network.
  // - net::ERR_CACHE_MISS
  //   The read was made too far away from the current buffered position.
  virtual void Read(int64 position, int read_size,
                    uint8* buffer, net::CompletionCallback* callback);

  // Returns the position of the last byte buffered. Returns
  // |kPositionNotSpecified| if such value is not available.
  virtual int64 GetBufferedPosition();

  // Gets the content length in bytes of the instance after this loader has been
  // started. If this value is |kPositionNotSpecified|, then content length is
  // unknown.
  virtual int64 content_length();

  // Gets the original size of the file requested. If this value is
  // |kPositionNotSpecified|, then the size is unknown.
  virtual int64 instance_size();

  // Returns true if the server supports byte range requests.
  virtual bool range_supported();

  // Returns true if network is currently active.
  virtual bool network_activity();

  // Returns resulting URL.
  virtual const GURL& url();

  // Used to inject a mock used for unittests.
  virtual void SetURLLoaderForTest(WebKit::WebURLLoader* mock_loader);

  // WebKit::WebURLLoaderClient implementation.
  virtual void willSendRequest(
      WebKit::WebURLLoader* loader,
      WebKit::WebURLRequest& newRequest,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void didSendData(
      WebKit::WebURLLoader* loader,
      unsigned long long bytesSent,
      unsigned long long totalBytesToBeSent);
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLResponse& response);
  virtual void didDownloadData(
      WebKit::WebURLLoader* loader,
      int data_length);
  virtual void didReceiveData(
      WebKit::WebURLLoader* loader,
      const char* data,
      int data_length,
      int encoded_data_length);
  virtual void didReceiveCachedMetadata(
      WebKit::WebURLLoader* loader,
      const char* data, int dataLength);
  virtual void didFinishLoading(
      WebKit::WebURLLoader* loader,
      double finishTime);
  virtual void didFail(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLError&);

  bool HasSingleOrigin() const;

  // Sets the defer strategy to the given value.
  void UpdateDeferStrategy(DeferStrategy strategy);

 protected:
  friend class base::RefCountedThreadSafe<BufferedResourceLoader>;
  virtual ~BufferedResourceLoader();

 private:
  friend class BufferedResourceLoaderTest;

  // Toggles whether the resource loading is deferred or not.
  // Returns true if a network event was fired.
  bool ToggleDeferring();

  // Returns true if we should defer resource loading, based
  // on current buffering scheme.
  bool ShouldEnableDefer();

  // Returns true if we should enable resource loading, based
  // on current buffering scheme.
  bool ShouldDisableDefer();

  // Updates deferring behavior based on current buffering scheme.
  void UpdateDeferBehavior();

  // Returns true if the current read request can be fulfilled by what is in
  // the buffer.
  bool CanFulfillRead();

  // Returns true if the current read request will be fulfilled in the future.
  bool WillFulfillRead();

  // Method that does the actual read and calls the |read_callback_|, assuming
  // the request range is in |buffer_|.
  void ReadInternal();

  // If we have made a range request, verify the response from the server.
  bool VerifyPartialResponse(const WebKit::WebURLResponse& response);

  // Returns the value for a range request header using parameters
  // |first_byte_position| and |last_byte_position|. Negative numbers other
  // than |kPositionNotSpecified| are not allowed for |first_byte_position| and
  // |last_byte_position|. |first_byte_position| should always be less than or
  // equal to |last_byte_position| if they are both not |kPositionNotSpecified|.
  // Empty string is returned on invalid parameters.
  std::string GenerateHeaders(int64 first_byte_position,
                              int64 last_byte_position);

  // Done with read. Invokes the read callback and reset parameters for the
  // read request.
  void DoneRead(int error);

  // Done with start. Invokes the start callback and reset it.
  void DoneStart(int error);

  // Calls |event_callback_| in terms of a network event.
  void NotifyNetworkEvent();

  bool HasPendingRead() { return read_callback_.get() != NULL; }

  // Helper function that returns true if a range request was specified.
  bool IsRangeRequest() const;

  // A sliding window of buffer.
  scoped_ptr<media::SeekableBuffer> buffer_;

  // True if resource loading was deferred.
  bool deferred_;

  // Current buffering algorithm in place for resource loading.
  DeferStrategy defer_strategy_;

  // True if resource loading has completed.
  bool completed_;

  // True if a range request was made.
  bool range_requested_;

  // True if Range header is supported.
  bool range_supported_;

  // Does the work of loading and sends data back to this client.
  scoped_ptr<WebKit::WebURLLoader> url_loader_;

  GURL url_;
  int64 first_byte_position_;
  int64 last_byte_position_;
  bool single_origin_;

  // Callback method that listens to network events.
  scoped_ptr<NetworkEventCallback> event_callback_;

  // Members used during request start.
  scoped_ptr<net::CompletionCallback> start_callback_;
  int64 offset_;
  int64 content_length_;
  int64 instance_size_;

  // Members used during a read operation. They should be reset after each
  // read has completed or failed.
  scoped_ptr<net::CompletionCallback> read_callback_;
  int64 read_position_;
  int read_size_;
  uint8* read_buffer_;

  // Offsets of the requested first byte and last byte in |buffer_|. They are
  // written by Read().
  int first_offset_;
  int last_offset_;

  // Used to ensure mocks for unittests are used instead of reset in Start().
  bool keep_test_loader_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoader);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_BUFFERED_RESOURCE_LOADER_H_
