// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor_client.h"
#include "media/base/pipeline.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "webkit/media/buffered_data_source.h"
#include "webkit/media/skcanvas_video_renderer.h"

class SkCanvas;

namespace base {
class MessageLoopProxy;
}

namespace gfx {
class Rect;
}

namespace media {
class VideoFrame;
class VideoRendererBase;
}

namespace webkit_media {

class WebMediaPlayerImpl;

// Acts as a thread proxy between the various threads used for multimedia and
// the render thread that WebMediaPlayerImpl is running on.
class WebMediaPlayerProxy
    : public base::RefCountedThreadSafe<WebMediaPlayerProxy>,
      public media::DecryptorClient {
 public:
  WebMediaPlayerProxy(const scoped_refptr<base::MessageLoopProxy>& render_loop,
                      WebMediaPlayerImpl* webmediaplayer);
  const scoped_refptr<BufferedDataSource>& data_source() {
    return data_source_;
  }
  void set_data_source(const scoped_refptr<BufferedDataSource>& data_source) {
    data_source_ = data_source;
  }

  // TODO(scherkus): remove this once VideoRendererBase::PaintCB passes
  // ownership of the VideoFrame http://crbug.com/108435
  void set_frame_provider(media::VideoRendererBase* frame_provider) {
    frame_provider_ = frame_provider;
  }

  // Methods for Filter -> WebMediaPlayerImpl communication.
  void Repaint();

  // Methods for WebMediaPlayerImpl -> Filter communication.
  void Paint(SkCanvas* canvas, const gfx::Rect& dest_rect, uint8_t alpha);
  void Detach();
  void GetCurrentFrame(scoped_refptr<media::VideoFrame>* frame_out);
  void PutCurrentFrame(scoped_refptr<media::VideoFrame> frame);
  bool HasSingleOrigin();
  bool DidPassCORSAccessCheck() const;

  void AbortDataSource();

  // DecryptorClient implementation.
  virtual void KeyAdded(const std::string& key_system,
                        const std::string& session_id) OVERRIDE;
  virtual void KeyError(const std::string& key_system,
                        const std::string& session_id,
                        media::Decryptor::KeyError error_code,
                        int system_code) OVERRIDE;
  virtual void KeyMessage(const std::string& key_system,
                          const std::string& session_id,
                          scoped_array<uint8> message,
                          int message_length,
                          const std::string& default_url) OVERRIDE;
  virtual void NeedKey(const std::string& key_system,
                       const std::string& session_id,
                       scoped_array<uint8> init_data,
                       int init_data_size) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<WebMediaPlayerProxy>;
  virtual ~WebMediaPlayerProxy();

  // Invoke |webmediaplayer_| to perform a repaint.
  void RepaintTask();

  // Notify |webmediaplayer_| that a key has been added.
  void KeyAddedTask(const std::string& key_system,
                    const std::string& session_id);

  // Notify |webmediaplayer_| that a key error occurred.
  void KeyErrorTask(const std::string& key_system,
                    const std::string& session_id,
                    media::Decryptor::KeyError error_code,
                    int system_code);

  // Notify |webmediaplayer_| that a key message has been generated.
  void KeyMessageTask(const std::string& key_system,
                      const std::string& session_id,
                      scoped_array<uint8> message,
                      int message_length,
                      const std::string& default_url);

  // Notify |webmediaplayer_| that a key is needed for decryption.
  void NeedKeyTask(const std::string& key_system,
                   const std::string& session_id,
                   scoped_array<uint8> init_data,
                   int init_data_size);

  // The render message loop where WebKit lives.
  scoped_refptr<base::MessageLoopProxy> render_loop_;
  WebMediaPlayerImpl* webmediaplayer_;

  scoped_refptr<BufferedDataSource> data_source_;
  scoped_refptr<media::VideoRendererBase> frame_provider_;
  SkCanvasVideoRenderer video_renderer_;

  base::Lock lock_;
  int outstanding_repaints_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerProxy);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_PROXY_H_
