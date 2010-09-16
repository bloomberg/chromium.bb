// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An extremely simple implementation of DataSource that downloads the entire
// media resource into memory before signaling that initialization has finished.
// Primarily used to test <audio> and <video> with buffering/caching removed
// from the equation.

#ifndef WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
#define WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"

class MessageLoop;
class WebMediaPlayerDelegateImpl;

namespace webkit_glue {

class SimpleDataSource : public media::DataSource,
                         public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  static media::FilterFactory* CreateFactory(
      MessageLoop* message_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory) {
    return new media::FilterFactoryImpl2<
        SimpleDataSource,
        MessageLoop*,
        webkit_glue::MediaResourceLoaderBridgeFactory*>(message_loop,
                                                        bridge_factory);
  }

  // media::FilterFactoryImpl2 implementation.
  static bool IsMediaFormatSupported(
      const media::MediaFormat& media_format);

  // MediaFilter implementation.
  virtual void Stop(media::FilterCallback* callback);

  // DataSource implementation.
  virtual void Initialize(const std::string& url,
                          media::FilterCallback* callback);
  virtual const media::MediaFormat& media_format();
  virtual void Read(int64 position, size_t size,
                    uint8* data, ReadCallback* read_callback);
  virtual bool GetSize(int64* size_out);
  virtual bool IsStreaming();

  // webkit_glue::ResourceLoaderBridge::Peer implementation.
  virtual void OnUploadProgress(uint64 position, uint64 size) {}
  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnDownloadedData(int len) {}
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info,
                                  const base::Time& completion_time);
  virtual GURL GetURLForDebugging() const;

 private:
  friend class media::FilterFactoryImpl2<
      SimpleDataSource,
      MessageLoop*,
      webkit_glue::MediaResourceLoaderBridgeFactory*>;
  SimpleDataSource(
      MessageLoop* render_loop,
      webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory);
  virtual ~SimpleDataSource();

  // Updates |url_| and |media_format_| with the given URL.
  void SetURL(const GURL& url);

  // Creates and starts the resource loading on the render thread.
  void StartTask();

  // Cancels and deletes the resource loading on the render thread.
  void CancelTask();

  // Perform initialization completion tasks under a lock.
  void DoneInitialization_Locked(bool success);

  // Primarily used for asserting the bridge is loading on the render thread.
  MessageLoop* render_loop_;

  // Factory to create a bridge.
  scoped_ptr<webkit_glue::MediaResourceLoaderBridgeFactory> bridge_factory_;

  // Bridge used to load the media resource.
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;

  media::MediaFormat media_format_;
  GURL url_;
  std::string data_;
  int64 size_;

  // Simple state tracking variable.
  enum State {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STOPPED,
  };
  State state_;

  // Used for accessing |state_|.
  Lock lock_;

  // Filter callbacks.
  scoped_ptr<media::FilterCallback> initialize_callback_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSource);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_SIMPLE_DATA_SOURCE_H_
