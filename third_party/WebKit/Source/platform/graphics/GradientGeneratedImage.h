/*
 * Copyright (C) 2008, 2012 Apple Computer, Inc.  All rights reserved.
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

#ifndef GradientGeneratedImage_h
#define GradientGeneratedImage_h

#include "platform/graphics/GeneratedImage.h"
#include "platform/graphics/Gradient.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class IntSize;

class PLATFORM_EXPORT GradientGeneratedImage final : public GeneratedImage {
 public:
  static RefPtr<GradientGeneratedImage> Create(RefPtr<Gradient> generator,
                                               const IntSize& size) {
    return WTF::AdoptRef(
        new GradientGeneratedImage(std::move(generator), size));
  }

  ~GradientGeneratedImage() override {}

  bool ApplyShader(PaintFlags&, const SkMatrix&) override;

 protected:
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&, const FloatRect&) override;

  GradientGeneratedImage(RefPtr<Gradient> generator, const IntSize& size)
      : GeneratedImage(size), gradient_(std::move(generator)) {}

  RefPtr<Gradient> gradient_;
};

}  // namespace blink

#endif
