// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_BUFFERED_DATA_SOURCE_H_
#define WEBKIT_MEDIA_BUFFERED_DATA_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "googleurl/src/gurl.h"
#include "media/base/data_source.h"
#include "media/base/pipeline_status.h"
#include "webkit/media/buffered_resource_loader.h"
#include "webkit/media/preload.h"

class MessageLoop;

namespace media {
class MediaLog;
}

namespace webkit_media {

// A data source capable of loading URLs and buffering the data using an
// in-memory sliding window.
//
// BufferedDataSource must be created and initialized on the render thread
// before being passed to other threads. It may be deleted on any thread.
class BufferedDataSource : public media::DataSource {
 public:
  BufferedDataSource(MessageLoop* render_loop,
                     WebKit::WebFrame* frame,
                     media::MediaLog* media_log);

  // Initialize this object using |url| and |cors_mode|, and call |status_cb|
  // when initialization has completed.
  //
  // Method called on the render thread.
  void Initialize(
      const GURL& url,
      BufferedResourceLoader::CORSMode cors_mode,
      const media::PipelineStatusCB& status_cb);

  // Adjusts the buffering algorithm based on the given preload value.
  void SetPreload(Preload preload);

  // Returns true if the media resource has a single origin, false otherwise.
  // Only valid to call after Initialize() has completed.
  //
  // Method called on the render thread.
  bool HasSingleOrigin();

  // Returns true if the media resource passed a CORS access control check.
  bool DidPassCORSAccessCheck() const;

  // Cancels initialization, any pending loaders, and any pending read calls
  // from the demuxer. The caller is expected to release its reference to this
  // object and never call it again.
  //
  // Method called on the render thread.
  void Abort();

  // media::DataSource implementation.
  // Called from demuxer thread.
  virtual void set_host(media::DataSourceHost* host) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;

  virtual void Read(int64 position, int size, uint8* data,
                    const media::DataSource::ReadCB& read_cb) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;
  virtual void SetBitrate(int bitrate) OVERRIDE;

 protected:
  virtual ~BufferedDataSource();

  // A factory method to create a BufferedResourceLoader based on the read
  // parameters. We can override this file to object a mock
  // BufferedResourceLoader for testing.
  virtual BufferedResourceLoader* CreateResourceLoader(
      int64 first_byte_position, int64 last_byte_position);

 private:
  friend class BufferedDataSourceTest;

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

  // Tells |loader_| the bitrate of the media.
  void SetBitrateTask(int bitrate);

  // The method that performs actual read. This method can only be executed on
  // the render thread.
  void ReadInternal();

  // Calls |read_cb_| and reset all read parameters. Non-negative |bytes_read|
  // values represent successful reads, otherwise |bytes_read| should be
  // kReadError.
  void DoneRead_Locked(int bytes_read);

  // Calls |initialize_cb_| and reset it.
  void DoneInitialization_Locked(media::PipelineStatus status);

  // Callback method for |loader_| if URL for the resource requested is using
  // HTTP protocol. This method is called when response for initial request is
  // received.
  void HttpInitialStartCallback(BufferedResourceLoader::Status status);

  // Callback method for |loader_| if URL for the resource requested is using
  // a non-HTTP protocol, e.g. local files. This method is called when response
  // for initial request is received.
  void NonHttpInitialStartCallback(BufferedResourceLoader::Status status);

  // Callback method to be passed to BufferedResourceLoader during range
  // request. Once a resource request has started, this method will be called
  // with the error code. This method will be executed on the thread
  // BufferedResourceLoader lives, i.e. render thread.
  void PartialReadStartCallback(BufferedResourceLoader::Status status);

  // Callback method for making a read request to BufferedResourceLoader.
  void ReadCallback(BufferedResourceLoader::Status status, int bytes_read);

  // Callback method when a network event is received.
  void NetworkEventCallback();

  void UpdateHostState_Locked();

  // URL of the resource requested.
  GURL url_;
  // crossorigin attribute on the corresponding HTML media element, if any.
  BufferedResourceLoader::CORSMode cors_mode_;

  // Members for total bytes of the requested object. It is written once on
  // render thread but may be read from any thread. However reading of this
  // member is guaranteed to happen after it is first written, so we don't
  // need to protect it.
  int64 total_bytes_;
  int64 buffered_bytes_;

  // This value will be true if this data source can only support streaming.
  // i.e. range request is not supported.
  bool streaming_;

  // A webframe for loading.
  WebKit::WebFrame* frame_;

  // A resource loader for the media resource.
  scoped_ptr<BufferedResourceLoader> loader_;

  // True if |loader| is downloading data.
  bool is_downloading_data_;

  // Callback method from the pipeline for initialization.
  media::PipelineStatusCB initialize_cb_;

  // Read parameters received from the Read() method call.
  media::DataSource::ReadCB read_cb_;
  int read_size_;
  uint8* read_buffer_;
  // Retained between reads to make sense of buffering information.
  int64 last_read_start_;

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

  // This variable is true when the user has requested the video to play at
  // least once.
  bool media_has_played_;

  // This variable holds the value of the preload attribute for the video
  // element.
  Preload preload_;

  // Number of cache miss retries left.
  int cache_miss_retries_left_;

  // Bitrate of the content, 0 if unknown.
  int bitrate_;

  // Current playback rate.
  float playback_rate_;

  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_BUFFERED_DATA_SOURCE_H_
