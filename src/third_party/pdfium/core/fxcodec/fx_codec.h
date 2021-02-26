// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_FX_CODEC_H_
#define CORE_FXCODEC_FX_CODEC_H_

#include <map>

#include "core/fxcrt/fx_safe_types.h"

namespace fxcodec {

#ifdef PDF_ENABLE_XFA
class CFX_DIBAttribute {
 public:
  CFX_DIBAttribute();
  ~CFX_DIBAttribute();

  int32_t m_nXDPI = -1;
  int32_t m_nYDPI = -1;
  uint16_t m_wDPIUnit = 0;
  std::map<uint32_t, void*> m_Exif;
};
#endif  // PDF_ENABLE_XFA

void ReverseRGB(uint8_t* pDestBuf, const uint8_t* pSrcBuf, int pixels);

FX_SAFE_UINT32 CalculatePitch8(uint32_t bpc, uint32_t components, int width);
FX_SAFE_UINT32 CalculatePitch32(int bpp, int width);

}  // namespace fxcodec

#ifdef PDF_ENABLE_XFA
using CFX_DIBAttribute = fxcodec::CFX_DIBAttribute;
#endif

#endif  // CORE_FXCODEC_FX_CODEC_H_
