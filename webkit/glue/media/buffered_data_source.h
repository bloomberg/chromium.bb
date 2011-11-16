// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/filter_factories.h"
#include "media/base/filters.h"
#include "webkit/glue/media/buffered_resource_loader.h"

namespace media {
class MediaLog;
}

namespace webkit_glue {

// This class may be created on any thread, and is callable from the render
// thread as well as media-specific threads.
class BufferedDataSource : public WebDataSource {
 public:
  // Creates a DataSourceFactory for building BufferedDataSource objects.
  static media::DataSourceFactory* CreateFactory(
      MessageLoop* render_loop,
      WebKit::WebFrame* frame,
      media::MediaLog* media_log,
      const WebDataSourceBuildObserverHack& build_observer);

  BufferedDataSource(MessageLoop* render_loop,
                     WebKit::WebFrame* frame,
                     media::MediaLog* media_log);

  virtual ~BufferedDataSource();

  // media::Filter implementation.
  virtual void set_host(media::FilterHost* host) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;

  // media::DataSource implementation.
  // Called from demuxer thread.
  virtual void Read(
      int64 position,
      size_t size,
      uint8* data,
      const media::DataSource::ReadCallback& read_callback) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;
  virtual void SetPreload(media::Preload preload) OVERRIDE;
  virtual void SetBitrate(int bitrate) OVERRIDE;

  // webkit_glue::WebDataSource implementation.
  virtual void Initialize(const std::string& url,
                          const media::PipelineStatusCB& callback) OVERRIDE;
  virtual void CancelInitialize() OVERRIDE;
  virtual bool HasSingleOrigin() OVERRIDE;
  virtual void Abort() OVERRIDE;

 protected:
  // A factory method to create a BufferedResourceLoader based on the read
  // parameters. We can override this file to object a mock
  // BufferedResourceLoader for testing.
  virtual BufferedResourceLoader* CreateResourceLoader(
      int64 first_byte_position, int64 last_byte_position);

 private:
  friend class BufferedDataSourceTest2;

  // Posted to perform initialization on render thread and start resource
  // loading.
  void InitializeTask();

  // Task posted to perform actual reading on the render thread.
  void ReadTask(int64 position, int read_size, uint8* read_buffer);

  // Task posted when Stop() is called. Stops |watch_dog_timer_| and
  // |loader_|, reset Read() variables, and set |stopped_on_render_loop_|
  // to signal any remaining tasks to stop.
  void CleanupTask();

  // Restart resource loading on render thread.
  void RestartLoadingTask();

  // This task uses the current playback rate with the previous playback rate
  // to determine whether we are going from pause to play and play to pause,
  // and signals the buffered resource loader accordingly.
  void SetPlaybackRateTask(float playback_rate);

  // This task saves the preload value for the media.
  void SetPreloadTask(media::Preload preload);

  // Tells |loader_| the bitrate of the media.
  void SetBitrateTask(int bitrate);

  // Decides which DeferStrategy to used based on the current state of the
  // BufferedDataSource.
  BufferedResourceLoader::DeferStrategy ChooseDeferStrategy();

  // The method that performs actual read. This method can only be executed on
  // the render thread.
  void ReadInternal();

  // Calls |read_callback_| and reset all read parameters.
  void DoneRead_Locked(int error);

  // Calls |initialize_cb_| and reset it.
  void DoneInitialization_Locked(media::PipelineStatus status);

  // Callback method for |loader_| if URL for the resource requested is using
  // HTTP protocol. This method is called when response for initial request is
  // received.
  void HttpInitialStartCallback(int error);

  // Callback method for |loader_| if URL for the resource requested is using
  // a non-HTTP protocol, e.g. local files. This method is called when response
  // for initial request is received.
  void NonHttpInitialStartCallback(int error);

  // Callback method to be passed to BufferedResourceLoader during range
  // request. Once a resource request has started, this method will be called
  // with the error code. This method will be executed on the thread
  // BufferedResourceLoader lives, i.e. render thread.
  void PartialReadStartCallback(int error);

  // Callback method for making a read request to BufferedResourceLoader.
  // If data arrives or the request has failed, this method is called with
  // the error code or the number of bytes read.
  void ReadCallback(int error);

  // Callback method when a network event is received.
  void NetworkEventCallback();

  void UpdateHostState_Locked();

  // URL of the resource requested.
  GURL url_;

  // Members for total bytes of the requested object. It is written once on
  // render thread but may be read from any thread. However reading of this
  // member is guaranteed to happen after it is first written, so we don't
  // need to protect it.
  int64 total_bytes_;
  int64 buffered_bytes_;

  // True if this data source is considered loaded.
  bool loaded_;

  // This value will be true if this data source can only support streaming.
  // i.e. range request is not supported.
  bool streaming_;

  // A webframe for loading.
  WebKit::WebFrame* frame_;

  // A resource loader for the media resource.
  scoped_refptr<BufferedResourceLoader> loader_;

  // True if |loader| is downloading data.
  bool is_downloading_data_;

  // Callback method from the pipeline for initialization.
  media::PipelineStatusCB initialize_cb_;

  // Read parameters received from the Read() method call.
  media::DataSource::ReadCallback read_callback_;
  int64 read_position_;
  int read_size_;
  uint8* read_buffer_;

  // This buffer is intermediate, we use it for BufferedResourceLoader to write
  // to. And when read in BufferedResourceLoader is done, we copy data from
  // this buffer to |read_buffer_|. The reason for an additional copy is that
  // we don't own |read_buffer_|. But since the read operation is asynchronous,
  // |read_buffer| can be destroyed at any time, so we only copy into
  // |read_buffer| in the final step when it is safe.
  // Memory is allocated for this member during initialization of this object
  // because we want buffer to be passed into BufferedResourceLoader to be
  // always non-null. And by initializing this member with a default size we can
  // avoid creating zero-sized buffered if the first read has zero size.
  scoped_array<uint8> intermediate_read_buffer_;
  int intermediate_read_buffer_size_;

  // The message loop of the render thread.
  MessageLoop* render_loop_;

  // Protects |stop_signal_received_|, |stopped_on_render_loop_| and
  // |initialize_cb_|.
  base::Lock lock_;

  // Stop signal to suppressing activities. This variable is set on the pipeline
  // thread and read from the render thread.
  bool stop_signal_received_;

  // This variable is set by CleanupTask() that indicates this object is stopped
  // on the render thread and work should no longer progress.
  bool stopped_on_render_loop_;

  // This variable is true when we are in a paused state and false when we
  // are in a playing state.
  bool media_is_paused_;

  // This variable is true when the user has requested the video to play at
  // least once.
  bool media_has_played_;

  // This variable holds the value of the preload attribute for the video
  // element.
  media::Preload preload_;

  // Keeps track of whether we used a Range header in the initialization
  // request.
  bool using_range_request_;

  // Number of cache miss retries left.
  int cache_miss_retries_left_;

  // Bitrate of the content, 0 if unknown.
  int bitrate_;

  // Current playback rate.
  float playback_rate_;

  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_
