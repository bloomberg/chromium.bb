// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRWebGLDrawingBuffer_h
#define XRWebGLDrawingBuffer_h

#include "cc/layers/texture_layer_client.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

class DrawingBuffer;
class StaticBitmapImage;

class PLATFORM_EXPORT XRWebGLDrawingBuffer
    : public RefCounted<XRWebGLDrawingBuffer> {
 public:
  static scoped_refptr<XRWebGLDrawingBuffer> Create(DrawingBuffer*,
                                                    GLuint framebuffer,
                                                    const IntSize&,
                                                    bool want_alpha_channel,
                                                    bool want_depth_buffer,
                                                    bool want_stencil_buffer,
                                                    bool want_antialiasing,
                                                    bool want_multiview);

  gpu::gles2::GLES2Interface* ContextGL();
  bool ContextLost();

  const IntSize& size() const { return size_; }

  bool antialias() const { return anti_aliasing_mode_ != kNone; }
  bool depth() const { return depth_; }
  bool stencil() const { return stencil_; }
  bool alpha() const { return alpha_; }
  bool multiview() const { return multiview_; }

  void Resize(const IntSize&);

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage(
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);

  class MirrorClient {
   public:
    virtual void OnMirrorImageAvailable(
        scoped_refptr<StaticBitmapImage>,
        std::unique_ptr<viz::SingleReleaseCallback>) = 0;
  };

  void SetMirrorClient(MirrorClient*);

 private:
  struct ColorBuffer : public RefCounted<ColorBuffer> {
    ColorBuffer(XRWebGLDrawingBuffer*, const IntSize&, GLuint texture_id);
    ~ColorBuffer();

    // The owning XRWebGLDrawingBuffer. Note that DrawingBuffer is explicitly
    // destroyed by the beginDestruction method, which will eventually drain all
    // of its ColorBuffers.
    scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer;
    const IntSize size;
    const GLuint texture_id = 0;

    // The mailbox used to send this buffer to the compositor.
    gpu::Mailbox mailbox;

    // The sync token for when this buffer was sent to the compositor.
    gpu::SyncToken produce_sync_token;

    // The sync token for when this buffer was received back from the
    // compositor.
    gpu::SyncToken receive_sync_token;

   private:
    WTF_MAKE_NONCOPYABLE(ColorBuffer);
  };

  XRWebGLDrawingBuffer(DrawingBuffer*,
                       GLuint framebuffer,
                       bool discard_framebuffer_supported,
                       bool want_alpha_channel,
                       bool want_depth_buffer,
                       bool want_stencil_buffer,
                       bool multiview_supported);

  bool Initialize(const IntSize&, bool use_multisampling, bool use_multiview);

  IntSize AdjustSize(const IntSize&);

  scoped_refptr<ColorBuffer> CreateColorBuffer();
  scoped_refptr<ColorBuffer> CreateOrRecycleColorBuffer();

  bool WantExplicitResolve() const;
  void SwapColorBuffers();

  void MailboxReleased(scoped_refptr<ColorBuffer>,
                       const gpu::SyncToken&,
                       bool lost_resource);
  void MailboxReleasedToMirror(scoped_refptr<ColorBuffer>,
                               const gpu::SyncToken&,
                               bool lost_resource);

  // Reference to the DrawingBuffer that owns the GL context for this object.
  scoped_refptr<DrawingBuffer> drawing_buffer_;

  const GLuint framebuffer_ = 0;
  GLuint resolved_framebuffer_ = 0;
  GLuint multisample_renderbuffer_ = 0;
  scoped_refptr<ColorBuffer> back_color_buffer_ = 0;
  scoped_refptr<ColorBuffer> front_color_buffer_ = 0;
  GLuint depth_stencil_buffer_ = 0;
  IntSize size_;

  // Color buffers that were released by the XR compositor can be used again.
  Deque<scoped_refptr<ColorBuffer>> recycled_color_buffer_queue_;

  bool discard_framebuffer_supported_;
  bool depth_;
  bool stencil_;
  bool alpha_;
  bool multiview_;

  enum AntialiasingMode {
    kNone,
    kMSAAImplicitResolve,
    kMSAAExplicitResolve,
    kScreenSpaceAntialiasing,
  };

  AntialiasingMode anti_aliasing_mode_ = kNone;

  bool storage_texture_supported_ = false;
  int max_texture_size_ = 0;
  int sample_count_ = 0;
  bool framebuffer_incomplete_ = false;

  MirrorClient* mirror_client_ = nullptr;
};

}  // namespace blink

#endif  // XRWebGLDrawingBuffer_h
