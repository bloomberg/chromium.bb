// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_GIF_GIF_DECODER_H_
#define CORE_FXCODEC_GIF_GIF_DECODER_H_

#include <memory>
#include <utility>

#include "core/fxcodec/gif/cfx_gif.h"
#include "core/fxcodec/progressive_decoder_iface.h"
#include "core/fxcrt/fx_coordinates.h"

#ifndef PDF_ENABLE_XFA_GIF
#error "GIF must be enabled"
#endif

namespace fxcodec {

class CFX_DIBAttribute;

class GifDecoder {
 public:
  class Delegate {
   public:
    virtual void GifRecordCurrentPosition(uint32_t& cur_pos) = 0;
    virtual bool GifInputRecordPositionBuf(uint32_t rcd_pos,
                                           const FX_RECT& img_rc,
                                           int32_t pal_num,
                                           CFX_GifPalette* pal_ptr,
                                           int32_t delay_time,
                                           bool user_input,
                                           int32_t trans_index,
                                           int32_t disposal_method,
                                           bool interlace) = 0;
    virtual void GifReadScanline(int32_t row_num, uint8_t* row_buf) = 0;
  };

  static std::unique_ptr<ProgressiveDecoderIface::Context> StartDecode(
      Delegate* pDelegate);
  static CFX_GifDecodeStatus ReadHeader(
      ProgressiveDecoderIface::Context* context,
      int* width,
      int* height,
      int* pal_num,
      CFX_GifPalette** pal_pp,
      int* bg_index);
  static std::pair<CFX_GifDecodeStatus, size_t> LoadFrameInfo(
      ProgressiveDecoderIface::Context* context);
  static CFX_GifDecodeStatus LoadFrame(
      ProgressiveDecoderIface::Context* context,
      size_t frame_num);
  static FX_FILESIZE GetAvailInput(ProgressiveDecoderIface::Context* context);
  static bool Input(ProgressiveDecoderIface::Context* context,
                    RetainPtr<CFX_CodecMemory> codec_memory,
                    CFX_DIBAttribute* pAttribute);

  GifDecoder() = delete;
  GifDecoder(const GifDecoder&) = delete;
  GifDecoder& operator=(const GifDecoder&) = delete;
};

}  // namespace fxcodec

using GifDecoder = fxcodec::GifDecoder;

#endif  // CORE_FXCODEC_GIF_GIF_DECODER_H_
