// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_DECODER_H_
#define CORE_FXCODEC_JBIG2_JBIG2_DECODER_H_

#include <memory>

#include "core/fxcodec/fx_codec_def.h"
#include "third_party/base/span.h"

class CJBig2_Context;
class JBig2_DocumentContext;
class PauseIndicatorIface;

namespace fxcodec {

class Jbig2Context {
 public:
  Jbig2Context();
  ~Jbig2Context();

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint64_t m_nGlobalKey = 0;
  uint64_t m_nSrcKey = 0;
  pdfium::span<const uint8_t> m_pGlobalSpan;
  pdfium::span<const uint8_t> m_pSrcSpan;
  uint8_t* m_dest_buf = nullptr;
  uint32_t m_dest_pitch = 0;
  std::unique_ptr<CJBig2_Context> m_pContext;
};

class Jbig2Decoder {
 public:
  static FXCODEC_STATUS StartDecode(
      Jbig2Context* pJbig2Context,
      JBig2_DocumentContext* pJbig2DocumentContext,
      uint32_t width,
      uint32_t height,
      pdfium::span<const uint8_t> src_span,
      uint64_t src_key,
      pdfium::span<const uint8_t> global_span,
      uint64_t global_key,
      uint8_t* dest_buf,
      uint32_t dest_pitch,
      PauseIndicatorIface* pPause);

  static FXCODEC_STATUS ContinueDecode(Jbig2Context* pJbig2Context,
                                       PauseIndicatorIface* pPause);

  Jbig2Decoder() = delete;
  Jbig2Decoder(const Jbig2Decoder&) = delete;
  Jbig2Decoder& operator=(const Jbig2Decoder&) = delete;
};

}  // namespace fxcodec

using Jbig2Context = fxcodec::Jbig2Context;
using Jbig2Decoder = fxcodec::Jbig2Decoder;

#endif  // CORE_FXCODEC_JBIG2_JBIG2_DECODER_H_
