/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGLTexture_h
#define WebGLTexture_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "public/platform/WebMediaPlayer.h"

namespace blink {

class WebGLTexture final : public WebGLSharedPlatform3DObject {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~WebGLTexture() override;

  static WebGLTexture* Create(WebGLRenderingContextBase*);

  void SetTarget(GLenum);

  GLenum GetTarget() const { return target_; }

  bool HasEverBeenBound() const { return Object() && target_; }

  static GLint ComputeLevelCount(GLsizei width, GLsizei height, GLsizei depth);

  void UpdateLastUploadedVideo(WebMediaPlayer*);
  unsigned lastUploadedVideoWidth() const { return last_uploaded_video_width_; }
  unsigned lastUploadedVideoHeight() const {
    return last_uploaded_video_height_;
  }
  double lastUploadedVideoTimestamp() const {
    return last_uploaded_video_timestamp_;
  }

 private:
  explicit WebGLTexture(WebGLRenderingContextBase*);

  void DeleteObjectImpl(gpu::gles2::GLES2Interface*) override;

  bool IsTexture() const override { return true; }

  int MapTargetToIndex(GLenum) const;

  GLenum target_;

  unsigned last_uploaded_video_width_ = 0;
  unsigned last_uploaded_video_height_ = 0;
  double last_uploaded_video_timestamp_ = 0.0;
};

}  // namespace blink

#endif  // WebGLTexture_h
