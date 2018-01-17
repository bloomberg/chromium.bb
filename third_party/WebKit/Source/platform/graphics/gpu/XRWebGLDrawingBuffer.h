// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRWebGLDrawingBuffer_h
#define XRWebGLDrawingBuffer_h

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class DrawingBuffer;
class StaticBitmapImage;

class PLATFORM_EXPORT XRWebGLDrawingBuffer {
 public:
  static XRWebGLDrawingBuffer* Create(DrawingBuffer*,
                                      GLuint framebuffer,
                                      const IntSize&,
                                      bool want_alpha_channel,
                                      bool want_depth_buffer,
                                      bool want_stencil_buffer,
                                      bool want_antialiasing,
                                      bool want_multiview);

  bool ContextLost();

  const IntSize& size() const { return size_; }

  bool antialias() const { return anti_aliasing_mode_ != kNone; }
  bool depth() const { return depth_; }
  bool stencil() const { return stencil_; }
  bool alpha() const { return alpha_; }
  bool multiview() const { return multiview_; }

  void Resize(const IntSize&);

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage();

 private:
  XRWebGLDrawingBuffer(DrawingBuffer*,
                       GLuint framebuffer,
                       bool discard_framebuffer_supported,
                       bool want_alpha_channel,
                       bool want_depth_buffer,
                       bool want_stencil_buffer,
                       bool multiview_supported);

  bool Initialize(const IntSize&, bool use_multisampling, bool use_multiview);

  GLuint CreateColorBuffer();

  bool WantExplicitResolve() const;
  void SwapColorBuffers();

  // Reference to the DrawingBuffer that owns the GL context for this object.
  scoped_refptr<DrawingBuffer> drawing_buffer_;

  const GLuint framebuffer_ = 0;
  GLuint resolved_framebuffer_ = 0;
  GLuint multisample_renderbuffer_ = 0;
  GLuint back_color_buffer_ = 0;
  GLuint front_color_buffer_ = 0;
  GLuint depth_stencil_buffer_ = 0;
  IntSize size_;

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
  int sample_count_ = 0;
};

}  // namespace blink

#endif  // XRWebGLDrawingBuffer_h
