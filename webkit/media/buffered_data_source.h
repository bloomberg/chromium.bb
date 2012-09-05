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
#include "media/base/ranges.h"
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
  typedef base::Callback<void(bool)> DownloadingCB;

  // |downloading_cb| will be called whenever the downloading/paused state of
  // the source changes.
  BufferedDataSource(MessageLoop* render_loop,
                     WebKit::WebFrame* frame,
                     media::MediaLog* media_log,
                     const DownloadingCB& downloading_cb);

  // Initialize this object using |url| and |cors_mode|, executing |init_cb|
  // with the result of initialization when it has completed.
  //
  // Method called on the render thread.
  typedef base::Callback<void(bool)> InitializeCB;
  void Initialize(
      const GURL& url,
      BufferedResourceLoader::CORSMode cors_mode,
      const InitializeCB& init_cb);

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

  // Task posted when Stop() is called. Stops |loader_|, resets Read()
  // variables, and sets |stopped_on_render_loop_| to signal any remaining
  // tasks to stop.
  void CleanupTask();

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

  // BufferedResourceLoader::Start() callback for initial load.
  void StartCallback(BufferedResourceLoader::Status status);

  // BufferedResourceLoader::Start() callback for subsequent loads (i.e.,
  // when accessing ranges that are outside initial buffered region).
  void PartialReadStartCallback(BufferedResourceLoader::Status status);

  // BufferedResourceLoader callbacks.
  void ReadCallback(BufferedResourceLoader::Status status, int bytes_read);
  void LoadingStateChangedCallback(BufferedResourceLoader::LoadingState state);
  void ProgressCallback(int64 position);

  // Report a buffered byte range [start,end] or queue it for later
  // reporting if set_host() hasn't been called yet.
  void ReportOrQueueBufferedBytes(int64 start, int64 end);

  void UpdateHostState_Locked();

  // URL of the resource requested.
  GURL url_;
  // crossorigin attribute on the corresponding HTML media element, if any.
  BufferedResourceLoader::CORSMode cors_mode_;

  // The total size of the resource. Set during StartCallback() if the size is
  // known, otherwise it will remain kPositionNotSpecified until the size is
  // determined by reaching EOF.
  int64 total_bytes_;

  // Some resources are assumed to be fully buffered (i.e., file://) so we don't
  // need to report what |loader_| has buffered.
  bool assume_fully_buffered_;

  // This value will be true if this data source can only support streaming.
  // i.e. range request is not supported.
  bool streaming_;

  // A webframe for loading.
  WebKit::WebFrame* frame_;

  // A resource loader for the media resource.
  scoped_ptr<BufferedResourceLoader> loader_;

  // Callback method from the pipeline for initialization.
  InitializeCB init_cb_;

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
  // |init_cb_|.
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

  // Buffered byte ranges awaiting set_host() being called to report to host().
  media::Ranges<int64> queued_buffered_byte_ranges_;

  scoped_refptr<media::MediaLog> media_log_;

  DownloadingCB downloading_cb_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_BUFFERED_DATA_SOURCE_H_
