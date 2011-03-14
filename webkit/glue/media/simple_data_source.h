// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An extremely simple implementation of DataSource that downloads the entire
// media resource into memory before signaling that initialization has finished.
// Primarily used to test <audio> and <video> with buffering/caching removed
// from the equation.

#ifndef WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_

#include <algorithm>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "media/base/filter_factories.h"
#include "media/base/filters.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/glue/media/web_data_source.h"

class MessageLoop;
class WebMediaPlayerDelegateImpl;

namespace webkit_glue {

class SimpleDataSource : public WebDataSource,
                         public WebKit::WebURLLoaderClient {
 public:
  // Creates a DataSourceFactory for building SimpleDataSource objects.
  static media::DataSourceFactory* CreateFactory(
      MessageLoop* render_loop,
      WebKit::WebFrame* frame,
      WebDataSourceBuildObserverHack* build_observer);

  SimpleDataSource(MessageLoop* render_loop, WebKit::WebFrame* frame);
  virtual ~SimpleDataSource();

  // media::Filter implementation.
  virtual void set_host(media::FilterHost* host);
  virtual void Stop(media::FilterCallback* callback);

  // media::DataSource implementation.
  virtual const media::MediaFormat& media_format();
  virtual void Read(int64 position, size_t size,
                    uint8* data, ReadCallback* read_callback);
  virtual bool GetSize(int64* size_out);
  virtual bool IsStreaming();

  // Used to inject a mock used for unittests.
  virtual void SetURLLoaderForTest(WebKit::WebURLLoader* mock_loader);

  // WebKit::WebURLLoaderClient implementations.
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
      int dataLength);
  virtual void didReceiveData(
      WebKit::WebURLLoader* loader,
      const char* data,
      int dataLength);
  virtual void didReceiveCachedMetadata(
      WebKit::WebURLLoader* loader,
      const char* data, int dataLength);
  virtual void didFinishLoading(
      WebKit::WebURLLoader* loader,
      double finishTime);
  virtual void didFail(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLError&);

  // webkit_glue::WebDataSource implementation.
  virtual void Initialize(const std::string& url,
                          media::PipelineStatusCallback* callback);
  virtual void CancelInitialize();
  virtual bool HasSingleOrigin();
  virtual void Abort();

 private:
  // Updates |url_| and |media_format_| with the given URL.
  void SetURL(const GURL& url);

  // Creates and starts the resource loading on the render thread.
  void StartTask();

  // Cancels and deletes the resource loading on the render thread.
  void CancelTask();

  // Perform initialization completion tasks under a lock.
  void DoneInitialization_Locked(bool success);

  // Update host() stats like total bytes & buffered bytes.
  void UpdateHostState();

  // Primarily used for asserting the bridge is loading on the render thread.
  MessageLoop* render_loop_;

  // A webframe for loading.
  WebKit::WebFrame* frame_;

  // Does the work of loading and sends data back to this client.
  scoped_ptr<WebKit::WebURLLoader> url_loader_;

  media::MediaFormat media_format_;
  GURL url_;
  std::string data_;
  int64 size_;
  bool single_origin_;

  // Simple state tracking variable.
  enum State {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STOPPED,
  };
  State state_;

  // Used for accessing |state_|.
  base::Lock lock_;

  // Filter callbacks.
  scoped_ptr<media::PipelineStatusCallback> initialize_callback_;

  // Used to ensure mocks for unittests are used instead of reset in Start().
  bool keep_test_loader_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
