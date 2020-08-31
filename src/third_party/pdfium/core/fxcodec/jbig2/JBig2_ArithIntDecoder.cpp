// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_ArithIntDecoder.h"

#include <vector>

#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_safe_types.h"

namespace {

int ShiftOr(int val, int bitwise_or_val) {
  return (val << 1) | bitwise_or_val;
}

const struct ArithIntDecodeData {
  int nNeedBits;
  int nValue;
} g_ArithIntDecodeData[] = {
    {2, 0}, {4, 4}, {6, 20}, {8, 84}, {12, 340}, {32, 4436},
};

size_t RecursiveDecode(CJBig2_ArithDecoder* decoder,
                       std::vector<JBig2ArithCtx>* context,
                       int* prev,
                       size_t depth) {
  static const size_t kDepthEnd = FX_ArraySize(g_ArithIntDecodeData) - 1;
  if (depth == kDepthEnd)
    return kDepthEnd;

  JBig2ArithCtx* pCX = &(*context)[*prev];
  int D = decoder->Decode(pCX);
  *prev = ShiftOr(*prev, D);
  if (!D)
    return depth;
  return RecursiveDecode(decoder, context, prev, depth + 1);
}

}  // namespace

CJBig2_ArithIntDecoder::CJBig2_ArithIntDecoder() {
  m_IAx.resize(512);
}

CJBig2_ArithIntDecoder::~CJBig2_ArithIntDecoder() {}

bool CJBig2_ArithIntDecoder::Decode(CJBig2_ArithDecoder* pArithDecoder,
                                    int* nResult) {
  // This decoding algorithm is explained in "Annex A - Arithmetic Integer
  // Decoding Procedure" on page 113 of the JBIG2 specification (ISO/IEC FCD
  // 14492).
  int PREV = 1;
  const int S = pArithDecoder->Decode(&m_IAx[PREV]);
  PREV = ShiftOr(PREV, S);

  const size_t nDecodeDataIndex =
      RecursiveDecode(pArithDecoder, &m_IAx, &PREV, 0);

  int nTemp = 0;
  for (int i = 0; i < g_ArithIntDecodeData[nDecodeDataIndex].nNeedBits; ++i) {
    int D = pArithDecoder->Decode(&m_IAx[PREV]);
    PREV = ShiftOr(PREV, D);
    if (PREV >= 256)
      PREV = (PREV & 511) | 256;
    nTemp = ShiftOr(nTemp, D);
  }
  FX_SAFE_INT32 safeValue = g_ArithIntDecodeData[nDecodeDataIndex].nValue;
  safeValue += nTemp;

  // Value does not fit in int.
  if (!safeValue.IsValid()) {
    *nResult = 0;
    return false;
  }

  int nValue = safeValue.ValueOrDie();
  if (S == 1 && nValue > 0)
    nValue = -nValue;

  *nResult = nValue;
  return S != 1 || nValue != 0;
}

CJBig2_ArithIaidDecoder::CJBig2_ArithIaidDecoder(unsigned char SBSYMCODELENA)
    : SBSYMCODELEN(SBSYMCODELENA) {
  m_IAID.resize(static_cast<size_t>(1) << SBSYMCODELEN);
}

CJBig2_ArithIaidDecoder::~CJBig2_ArithIaidDecoder() {}

void CJBig2_ArithIaidDecoder::Decode(CJBig2_ArithDecoder* pArithDecoder,
                                     uint32_t* nResult) {
  int PREV = 1;
  for (unsigned char i = 0; i < SBSYMCODELEN; ++i) {
    JBig2ArithCtx* pCX = &m_IAID[PREV];
    int D = pArithDecoder->Decode(pCX);
    PREV = ShiftOr(PREV, D);
  }
  *nResult = PREV - (1 << SBSYMCODELEN);
}
