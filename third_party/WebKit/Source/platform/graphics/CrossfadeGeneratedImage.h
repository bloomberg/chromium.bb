/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef CrossfadeGeneratedImage_h
#define CrossfadeGeneratedImage_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/GeneratedImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT CrossfadeGeneratedImage final : public GeneratedImage {
 public:
  static RefPtr<CrossfadeGeneratedImage> Create(RefPtr<Image> from_image,
                                                RefPtr<Image> to_image,
                                                float percentage,
                                                IntSize crossfade_size,
                                                const IntSize& size) {
    return WTF::AdoptRef(
        new CrossfadeGeneratedImage(std::move(from_image), std::move(to_image),
                                    percentage, crossfade_size, size));
  }

  bool UsesContainerSize() const override { return false; }
  bool HasRelativeSize() const override { return false; }

  IntSize Size() const override { return crossfade_size_; }

 protected:
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&, const FloatRect&) final;

  CrossfadeGeneratedImage(RefPtr<Image> from_image,
                          RefPtr<Image> to_image,
                          float percentage,
                          IntSize crossfade_size,
                          const IntSize&);

 private:
  void DrawCrossfade(PaintCanvas*,
                     const PaintFlags&,
                     ImageClampingMode,
                     ImageDecodingMode);

  RefPtr<Image> from_image_;
  RefPtr<Image> to_image_;

  float percentage_;
  IntSize crossfade_size_;
};

}  // namespace blink

#endif
