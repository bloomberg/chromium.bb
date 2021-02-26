// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JPEG_JPEG_PROGRESSIVE_DECODER_H_
#define CORE_FXCODEC_JPEG_JPEG_PROGRESSIVE_DECODER_H_

#include <csetjmp>
#include <memory>

#include "core/fxcodec/progressive_decoder_iface.h"
#include "third_party/base/no_destructor.h"

namespace fxcodec {

class CFX_DIBAttribute;

class JpegProgressiveDecoder final : public ProgressiveDecoderIface {
 public:
  static JpegProgressiveDecoder* GetInstance();

  static std::unique_ptr<Context> Start();

  static jmp_buf& GetJumpMark(Context* pContext);

  static int ReadHeader(Context* pContext,
                        int* width,
                        int* height,
                        int* nComps,
                        CFX_DIBAttribute* pAttribute);

  static bool StartScanline(Context* pContext, int down_scale);
  static bool ReadScanline(Context* pContext, uint8_t* dest_buf);

  // ProgressiveDecoderIface:
  FX_FILESIZE GetAvailInput(Context* pContext) const override;
  bool Input(Context* pContext,
             RetainPtr<CFX_CodecMemory> codec_memory,
             CFX_DIBAttribute* pAttribute) override;

 private:
  friend pdfium::base::NoDestructor<JpegProgressiveDecoder>;

  JpegProgressiveDecoder();
  ~JpegProgressiveDecoder() override;
};

}  // namespace fxcodec

using JpegProgressiveDecoder = fxcodec::JpegProgressiveDecoder;

#endif  // CORE_FXCODEC_JPEG_JPEG_PROGRESSIVE_DECODER_H_
