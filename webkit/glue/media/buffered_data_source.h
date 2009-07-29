// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_

#include <string>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "base/condition_variable.h"
#include "googleurl/src/gurl.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "media/base/seekable_buffer.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

namespace webkit_glue {

/////////////////////////////////////////////////////////////////////////////
// BufferedResourceLoader
// This class works inside demuxer thread and render thread. It contains a
// resource loader bridge and does the actual resource loading. This object
// does buffering internally, it defers the resource loading if buffer is
// full and un-defers the resource loading if it is under buffered.
class BufferedResourceLoader : public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  // |bridge_factory| - Factory to create a ResourceLoaderBridge.
  // |url| - URL for the resource to be loaded.
  // |first_byte_position| - First byte to start loading from, -1 for not
  // specified.
  // |last_byte_position| - Last byte to be loaded, -1 for not specified.
  BufferedResourceLoader(
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory,
      const GURL& url,
      int64 first_byte_position,
      int64 last_byte_position);
  virtual ~BufferedResourceLoader();

  // Start the resource loading with the specified URL and range.
  // This method operates in asynchronous mode. Once there's a response from the
  // server, success or fail |start_callback| is called with the result.
  virtual void Start(net::CompletionCallback* callback);

  // Stop this loader, cancels and request and release internal buffer.
  virtual void Stop();

  // Reads the specified |read_size| from |position| into |buffer| and when
  // the operation is done invoke |callback| with number of bytes read or an
  // error code.
  virtual void Read(int64 position, int read_size,
                    uint8* buffer, net::CompletionCallback* callback);

  // Gets the content length in bytes of the instance after this loader has been
  // started.
  virtual int64 content_length() { return content_length_; }

  /////////////////////////////////////////////////////////////////////////////
  // webkit_glue::ResourceLoaderBridge::Peer implementations.
  virtual void OnUploadProgress(uint64 position, uint64 size) {}
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
      const std::string& security_info);
  std::string GetURLForDebugging() { return url_.spec(); }

 protected:
  // An empty constructor so mock classes can be constructed.
  BufferedResourceLoader() {
  }

 private:
  // Defer the resource loading if the buffer is full.
  void EnableDeferIfNeeded();

  // Disable defer loading if we are under-buffered.
  void DisableDeferIfNeeded();

  // Returns true if the current read request can be fulfilled by what is in
  // the buffer.
  bool CanFulfillRead();

  // Returns true if the current read request will be fulfilled in the future.
  bool WillFulfillRead();

  // Checks parameters and make sure they are valid.
  bool VerifyRead();

  // Method that does the actual read and calls the |read_callbac_|, assuming
  // the request range is in |buffer_|.
  void ReadInternal();

  // Done with read. Invokes the read callback and reset parameters for the
  // read request.
  void DoneRead(int error);

  // Done with start. Invokes the start callback and reset it.
  void DoneStart(int error);

  bool HasPendingRead() { return read_callback_.get() != NULL; }

  // A sliding window of buffer.
  scoped_ptr<media::SeekableBuffer> buffer_;

  bool deferred_;
  bool completed_;
  bool range_requested_;

  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory_;
  GURL url_;
  int64 first_byte_position_;
  int64 last_byte_position_;

  // Members used during request start.
  scoped_ptr<net::CompletionCallback> start_callback_;
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;
  int64 offset_;
  int64 content_length_;

  // Members used during a read operation. They should be reset after each
  // read has completed or failed.
  scoped_ptr<net::CompletionCallback> read_callback_;
  int64 read_position_;
  int read_size_;
  uint8* read_buffer_;

  // Offsets of the requested first byte and last byte in |buffer_|. They are
  // written by VerifyRead().
  int first_offset_;
  int last_offset_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoader);
};

class BufferedDataSource : public media::DataSource {
 public:
  // Methods called from pipeline thread
  // Static methods for creating this class.
  static media::FilterFactory* CreateFactory(
      MessageLoop* message_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory) {
    return new media::FilterFactoryImpl2<
        BufferedDataSource,
        MessageLoop*,
        webkit_glue::MediaResourceLoaderBridgeFactory*>(
        message_loop, bridge_factory);
  }

  // media::MediaFilter implementation.
  virtual void Initialize(const std::string& url,
                          media::FilterCallback* callback);
  virtual void Stop();

  // media::DataSource implementation.
  // Called from demuxer thread.
  virtual void Read(int64 position, size_t size,
                    uint8* data,
                    media::DataSource::ReadCallback* read_callback);
  virtual bool GetSize(int64* size_out);
  virtual bool IsSeekable();

  const media::MediaFormat& media_format() {
    return media_format_;
  }

 protected:
  BufferedDataSource(
      MessageLoop* render_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory);
  virtual ~BufferedDataSource();

  // A factory method to create a BufferedResourceLoader based on the read
  // parameters. We can override this file to object a mock
  // BufferedResourceLoader for testing.
  virtual BufferedResourceLoader* CreateLoader(int64 first_byte_position,
                                               int64 last_byte_position);

  // Gets the number of milliseconds to declare a request timeout since
  // the request was made. This method is made virtual so as to inject a
  // different number for testing purpose.
  virtual base::TimeDelta GetTimeoutMilliseconds();

 private:
  friend class media::FilterFactoryImpl2<
      BufferedDataSource,
      MessageLoop*,
      webkit_glue::MediaResourceLoaderBridgeFactory*>;

  // Posted to perform initialization on render thread.
  void InitializeTask();

  // Task posted to perform resource loading and actual reading on the render
  // thread.
  void ReadTask(int64 position, int read_size,
                uint8* read_buffer,
                media::DataSource::ReadCallback* read_callback);

  // Task posted when Stop() is called.
  void StopTask();

  // Reset |loader_| with |loader| and starts it. This task is posted from
  // callback method from the current buffered resource loader.
  void SwapLoaderTask(BufferedResourceLoader* loader);

  // This task monitors the current active read request. If the current read
  // request has timed out, this task will destroy the current loader and
  // creates a new to accomodate the read request.
  void WatchDogTask();

  // The method that performs actual read. This method can only be executed on
  // the render thread.
  void ReadInternal();

  // Calls |read_callback_| and reset all read parameters.
  void DoneRead(int error);

  // Calls |initialize_callback_| and reset it.
  void DoneInitialization();

  // Callback method to perform BufferedResourceLoader::Start() during
  // initialization.
  void InitializeStartCallback(int error);

  // Callback method to be passed to BufferedResourceLoader during range
  // request. Once a resource request has started, this method will be called
  // with the error code. This method will be executed on the thread
  // BufferedResourceLoader lives, i.e. render thread.
  void PartialReadStartCallback(int error);

  // Callback method for making a read request to BufferedResourceLoader.
  // If data arrives or the request has failed, this method is called with
  // the error code or the number of bytes read.
  void ReadCallback(int error);

  media::MediaFormat media_format_;

  // URL of the resource requested.
  GURL url_;

  // Members for total bytes of the requested object. It is written once on
  // render thread but may be read from any thread. However reading of this
  // member is guaranteed to happen after it is first written, so we don't
  // need to protect it.
  int64 total_bytes_;

  // A factory object to produce ResourceLoaderBridge.
  scoped_ptr<webkit_glue::MediaResourceLoaderBridgeFactory> bridge_factory_;

  // A downloader object for loading the media resource.
  scoped_ptr<BufferedResourceLoader> loader_;

  // Read parameters received from the Read() method call.
  scoped_ptr<media::DataSource::ReadCallback> read_callback_;
  int64 read_position_;
  int read_size_;
  uint8* read_buffer_;
  base::Time read_submitted_time_;
  int read_attempts_;

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

  // Filter callbacks.
  scoped_ptr<media::FilterCallback> initialize_callback_;

  // Protects |stopped_|.
  Lock lock_;

  // Stop signal to suppressing activities.
  bool stopped_;

  // This timer is to run the WatchDogTask repeatedly. We use a timer instead
  // of doing PostDelayedTask() reduce the extra reference held by the message
  // loop. The RepeatingTimer does PostDelayedTask() internally, by using it
  // the message loop doesn't hold a reference for the watch dog task.
  base::RepeatingTimer<BufferedDataSource> watch_dog_timer_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_BUFFERED_DATA_SOURCE_H_
