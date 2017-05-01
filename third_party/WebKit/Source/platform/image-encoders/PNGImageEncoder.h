/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PNGImageEncoder_h
#define PNGImageEncoder_h

#include "platform/geometry/IntSize.h"
extern "C" {
#include "png.h"
}
#include <memory>
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

struct ImageDataBuffer;

class PLATFORM_EXPORT PNGImageEncoderState final {
  USING_FAST_MALLOC(PNGImageEncoderState);
  WTF_MAKE_NONCOPYABLE(PNGImageEncoderState);

 public:
  static std::unique_ptr<PNGImageEncoderState> Create(
      const IntSize& image_size,
      Vector<unsigned char>* output);
  ~PNGImageEncoderState();
  png_struct* Png() {
    DCHECK(png_);
    return png_;
  }
  png_info* Info() {
    DCHECK(info_);
    return info_;
  }

 private:
  PNGImageEncoderState(png_struct* png, png_info* info)
      : png_(png), info_(info) {}
  png_struct* png_;
  png_info* info_;
};

class PLATFORM_EXPORT PNGImageEncoder {
  STATIC_ONLY(PNGImageEncoder);

 public:
  // Encode the input data with default compression quality. See also
  // https://crbug.com/179289
  static bool Encode(const ImageDataBuffer&, Vector<unsigned char>* output);

  // Functions for progressive image encoding
  static void WriteOneRowToPng(unsigned char* pixels, PNGImageEncoderState*);
  static void FinalizePng(PNGImageEncoderState*);
};

}  // namespace blink

#endif
