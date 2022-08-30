// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/crt/cfgas_decimal.h"

#include <math.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "core/fxcrt/fx_extension.h"
#include "third_party/base/check.h"

#define FXMATH_DECIMAL_SCALELIMIT 0x1c
#define FXMATH_DECIMAL_RSHIFT32BIT(x) ((x) >> 0x10 >> 0x10)
#define FXMATH_DECIMAL_LSHIFT32BIT(x) ((x) << 0x10 << 0x10)

namespace {

inline uint8_t decimal_helper_div10(uint64_t& phi,
                                    uint64_t& pmid,
                                    uint64_t& plo) {
  uint8_t retVal;
  pmid += FXMATH_DECIMAL_LSHIFT32BIT(phi % 0xA);
  phi /= 0xA;
  plo += FXMATH_DECIMAL_LSHIFT32BIT(pmid % 0xA);
  pmid /= 0xA;
  retVal = plo % 0xA;
  plo /= 0xA;
  return retVal;
}

inline uint8_t decimal_helper_div10_any(uint64_t nums[], uint8_t numcount) {
  uint8_t retVal = 0;
  for (int i = numcount - 1; i > 0; i--) {
    nums[i - 1] += FXMATH_DECIMAL_LSHIFT32BIT(nums[i] % 0xA);
    nums[i] /= 0xA;
  }
  if (numcount) {
    retVal = nums[0] % 0xA;
    nums[0] /= 0xA;
  }
  return retVal;
}

inline void decimal_helper_mul10(uint64_t& phi, uint64_t& pmid, uint64_t& plo) {
  plo *= 0xA;
  pmid = pmid * 0xA + FXMATH_DECIMAL_RSHIFT32BIT(plo);
  plo = (uint32_t)plo;
  phi = phi * 0xA + FXMATH_DECIMAL_RSHIFT32BIT(pmid);
  pmid = (uint32_t)pmid;
}

inline void decimal_helper_mul10_any(uint64_t nums[], uint8_t numcount) {
  nums[0] *= 0xA;
  for (int i = 1; i < numcount; i++) {
    nums[i] = nums[i] * 0xA + FXMATH_DECIMAL_RSHIFT32BIT(nums[i - 1]);
    nums[i - 1] = (uint32_t)nums[i - 1];
  }
}

inline void decimal_helper_normalize(uint64_t& phi,
                                     uint64_t& pmid,
                                     uint64_t& plo) {
  phi += FXMATH_DECIMAL_RSHIFT32BIT(pmid);
  pmid = (uint32_t)pmid;
  pmid += FXMATH_DECIMAL_RSHIFT32BIT(plo);
  plo = (uint32_t)plo;
  phi += FXMATH_DECIMAL_RSHIFT32BIT(pmid);
  pmid = (uint32_t)pmid;
}

inline void decimal_helper_normalize_any(uint64_t nums[], uint8_t len) {
  for (int i = len - 2; i > 0; i--) {
    nums[i + 1] += FXMATH_DECIMAL_RSHIFT32BIT(nums[i]);
    nums[i] = (uint32_t)nums[i];
  }
  for (int i = 0; i < len - 1; i++) {
    nums[i + 1] += FXMATH_DECIMAL_RSHIFT32BIT(nums[i]);
    nums[i] = (uint32_t)nums[i];
  }
}

inline int8_t decimal_helper_raw_compare_any(uint64_t a[],
                                             uint8_t al,
                                             uint64_t b[],
                                             uint8_t bl) {
  int8_t retVal = 0;
  for (int i = std::max(al - 1, bl - 1); i >= 0; i--) {
    uint64_t l = (i >= al ? 0 : a[i]), r = (i >= bl ? 0 : b[i]);
    retVal += (l > r ? 1 : (l < r ? -1 : 0));
    if (retVal)
      return retVal;
  }
  return retVal;
}

inline void decimal_helper_dec_any(uint64_t a[], uint8_t al) {
  for (int i = 0; i < al; i++) {
    if (a[i]--)
      return;
  }
}

inline void decimal_helper_inc_any(uint64_t a[], uint8_t al) {
  for (int i = 0; i < al; i++) {
    a[i]++;
    if ((uint32_t)a[i] == a[i])
      return;
    a[i] = 0;
  }
}

inline void decimal_helper_raw_mul(uint64_t a[],
                                   uint8_t al,
                                   uint64_t b[],
                                   uint8_t bl,
                                   uint64_t c[],
                                   uint8_t cl) {
  DCHECK(al + bl <= cl);
  for (int i = 0; i < cl; i++)
    c[i] = 0;

  for (int i = 0; i < al; i++) {
    for (int j = 0; j < bl; j++) {
      uint64_t m = (uint64_t)a[i] * b[j];
      c[i + j] += (uint32_t)m;
      c[i + j + 1] += FXMATH_DECIMAL_RSHIFT32BIT(m);
    }
  }
  for (int i = 0; i < cl - 1; i++) {
    c[i + 1] += FXMATH_DECIMAL_RSHIFT32BIT(c[i]);
    c[i] = (uint32_t)c[i];
  }
  for (int i = 0; i < cl; i++)
    c[i] = (uint32_t)c[i];
}

inline void decimal_helper_raw_div(uint64_t a[],
                                   uint8_t al,
                                   uint64_t b[],
                                   uint8_t bl,
                                   uint64_t c[],
                                   uint8_t cl) {
  for (int i = 0; i < cl; i++)
    c[i] = 0;

  uint64_t left[16] = {0};
  uint64_t right[16] = {0};
  left[0] = 0;
  for (int i = 0; i < al; i++)
    right[i] = a[i];

  uint64_t tmp[16];
  while (decimal_helper_raw_compare_any(left, al, right, al) <= 0) {
    uint64_t cur[16];
    for (int i = 0; i < al; i++)
      cur[i] = left[i] + right[i];

    for (int i = al - 1; i >= 0; i--) {
      if (i)
        cur[i - 1] += FXMATH_DECIMAL_LSHIFT32BIT(cur[i] % 2);
      cur[i] /= 2;
    }

    decimal_helper_raw_mul(cur, al, b, bl, tmp, 16);
    switch (decimal_helper_raw_compare_any(tmp, 16, a, al)) {
      case -1:
        for (int i = 0; i < 16; i++)
          left[i] = cur[i];

        left[0]++;
        decimal_helper_normalize_any(left, al);
        break;
      case 1:
        for (int i = 0; i < 16; i++)
          right[i] = cur[i];
        decimal_helper_dec_any(right, al);
        break;
      case 0:
        for (int i = 0; i < std::min(al, cl); i++)
          c[i] = cur[i];
        return;
    }
  }
  for (int i = 0; i < std::min(al, cl); i++)
    c[i] = left[i];
}

inline bool decimal_helper_outofrange(uint64_t a[], uint8_t al, uint8_t goal) {
  for (int i = goal; i < al; i++) {
    if (a[i])
      return true;
  }
  return false;
}

inline void decimal_helper_shrinkintorange(uint64_t a[],
                                           uint8_t al,
                                           uint8_t goal,
                                           uint8_t& scale) {
  bool bRoundUp = false;
  while (scale != 0 && (scale > FXMATH_DECIMAL_SCALELIMIT ||
                        decimal_helper_outofrange(a, al, goal))) {
    bRoundUp = decimal_helper_div10_any(a, al) >= 5;
    scale--;
  }
  if (bRoundUp) {
    decimal_helper_normalize_any(a, goal);
    decimal_helper_inc_any(a, goal);
  }
}

inline void decimal_helper_truncate(uint64_t& phi,
                                    uint64_t& pmid,
                                    uint64_t& plo,
                                    uint8_t& scale,
                                    uint8_t minscale = 0) {
  while (scale > minscale) {
    uint64_t thi = phi, tmid = pmid, tlo = plo;
    if (decimal_helper_div10(thi, tmid, tlo) != 0)
      break;

    phi = thi;
    pmid = tmid;
    plo = tlo;
    scale--;
  }
}

}  // namespace

CFGAS_Decimal::CFGAS_Decimal() = default;

CFGAS_Decimal::CFGAS_Decimal(uint64_t val)
    : m_uMid(static_cast<uint32_t>(FXMATH_DECIMAL_RSHIFT32BIT(val))),
      m_uLo(static_cast<uint32_t>(val)) {}

CFGAS_Decimal::CFGAS_Decimal(uint32_t val)
    : m_uLo(static_cast<uint32_t>(val)) {}

CFGAS_Decimal::CFGAS_Decimal(uint32_t lo,
                             uint32_t mid,
                             uint32_t hi,
                             bool neg,
                             uint8_t scale)
    : m_uHi(hi),
      m_uMid(mid),
      m_uLo(lo),
      m_bNeg(neg && IsNotZero()),
      m_uScale(scale > FXMATH_DECIMAL_SCALELIMIT ? 0 : scale) {}

CFGAS_Decimal::CFGAS_Decimal(int32_t val) {
  if (val >= 0) {
    *this = CFGAS_Decimal(static_cast<uint32_t>(val));
  } else if (val == std::numeric_limits<int32_t>::min()) {
    *this = CFGAS_Decimal(static_cast<uint32_t>(val));
    SetNegate();
  } else {
    *this = CFGAS_Decimal(static_cast<uint32_t>(-val));
    SetNegate();
  }
}

CFGAS_Decimal::CFGAS_Decimal(float val, uint8_t scale) {
  float newval = fabs(val);
  float divisor = powf(2.0, 64.0f);
  uint64_t bottom64 = static_cast<uint64_t>(fmodf(newval, divisor));
  uint64_t top64 = static_cast<uint64_t>(newval / divisor);
  uint64_t plo = bottom64 & 0xFFFFFFFF;
  uint64_t pmid = bottom64 >> 32;
  uint64_t phi = top64 & 0xFFFFFFFF;

  newval = fmodf(newval, 1.0f);
  for (uint8_t iter = 0; iter < scale; iter++) {
    decimal_helper_mul10(phi, pmid, plo);
    newval *= 10;
    plo += static_cast<uint64_t>(newval);
    newval = fmodf(newval, 1.0f);
  }

  plo += FXSYS_roundf(newval);
  decimal_helper_normalize(phi, pmid, plo);
  m_uHi = static_cast<uint32_t>(phi);
  m_uMid = static_cast<uint32_t>(pmid);
  m_uLo = static_cast<uint32_t>(plo);
  m_bNeg = val < 0 && IsNotZero();
  m_uScale = scale;
}

CFGAS_Decimal::CFGAS_Decimal(WideStringView strObj) {
  const wchar_t* str = strObj.unterminated_c_str();
  const wchar_t* strBound = str + strObj.GetLength();
  bool pointmet = false;
  bool negmet = false;
  uint8_t scale = 0;
  m_uHi = 0;
  m_uMid = 0;
  m_uLo = 0;
  while (str != strBound && *str == ' ')
    str++;
  if (str != strBound && *str == '-') {
    negmet = true;
    str++;
  } else if (str != strBound && *str == '+') {
    str++;
  }

  while (str != strBound && (FXSYS_IsDecimalDigit(*str) || *str == '.') &&
         scale < FXMATH_DECIMAL_SCALELIMIT) {
    if (*str == '.') {
      if (!pointmet)
        pointmet = true;
    } else {
      m_uHi = m_uHi * 0xA + FXMATH_DECIMAL_RSHIFT32BIT((uint64_t)m_uMid * 0xA);
      m_uMid = m_uMid * 0xA + FXMATH_DECIMAL_RSHIFT32BIT((uint64_t)m_uLo * 0xA);
      m_uLo = m_uLo * 0xA + (*str - '0');
      if (pointmet)
        scale++;
    }
    str++;
  }
  m_bNeg = negmet && IsNotZero();
  m_uScale = scale;
}

WideString CFGAS_Decimal::ToWideString() const {
  WideString retString;
  WideString tmpbuf;
  uint64_t phi = m_uHi;
  uint64_t pmid = m_uMid;
  uint64_t plo = m_uLo;
  while (phi || pmid || plo)
    tmpbuf += decimal_helper_div10(phi, pmid, plo) + '0';

  uint8_t outputlen = (uint8_t)tmpbuf.GetLength();
  uint8_t scale = m_uScale;
  while (scale >= outputlen) {
    tmpbuf += '0';
    outputlen++;
  }
  if (m_bNeg && IsNotZero())
    retString += '-';

  for (uint8_t idx = 0; idx < outputlen; idx++) {
    if (idx == (outputlen - scale) && scale != 0)
      retString += '.';
    retString += tmpbuf[outputlen - 1 - idx];
  }
  return retString;
}

float CFGAS_Decimal::ToFloat() const {
  return static_cast<float>(ToDouble());
}

double CFGAS_Decimal::ToDouble() const {
  double pow = (double)(1 << 16) * (1 << 16);
  double base = static_cast<double>(m_uHi) * pow * pow +
                static_cast<double>(m_uMid) * pow + static_cast<double>(m_uLo);
  return (m_bNeg ? -1 : 1) * base * powf(10.0f, -m_uScale);
}

void CFGAS_Decimal::SetScale(uint8_t newscale) {
  uint8_t oldscale = m_uScale;
  if (oldscale == newscale)
    return;

  uint64_t phi = m_uHi;
  uint64_t pmid = m_uMid;
  uint64_t plo = m_uLo;
  if (newscale > oldscale) {
    for (uint8_t iter = 0; iter < newscale - oldscale; iter++)
      decimal_helper_mul10(phi, pmid, plo);

    m_uHi = static_cast<uint32_t>(phi);
    m_uMid = static_cast<uint32_t>(pmid);
    m_uLo = static_cast<uint32_t>(plo);
    m_bNeg = m_bNeg && IsNotZero();
    m_uScale = newscale;
  } else {
    uint64_t point5_hi = 0;
    uint64_t point5_mid = 0;
    uint64_t point5_lo = 5;
    for (uint8_t iter = 0; iter < oldscale - newscale - 1; iter++)
      decimal_helper_mul10(point5_hi, point5_mid, point5_lo);

    phi += point5_hi;
    pmid += point5_mid;
    plo += point5_lo;
    decimal_helper_normalize(phi, pmid, plo);
    for (uint8_t iter = 0; iter < oldscale - newscale; iter++)
      decimal_helper_div10(phi, pmid, plo);
  }
  m_uHi = static_cast<uint32_t>(phi);
  m_uMid = static_cast<uint32_t>(pmid);
  m_uLo = static_cast<uint32_t>(plo);
  m_bNeg = m_bNeg && IsNotZero();
  m_uScale = newscale;
}

void CFGAS_Decimal::SetNegate() {
  if (IsNotZero())
    m_bNeg = !m_bNeg;
}

CFGAS_Decimal CFGAS_Decimal::operator*(const CFGAS_Decimal& val) const {
  uint64_t a[3] = {m_uLo, m_uMid, m_uHi},
           b[3] = {val.m_uLo, val.m_uMid, val.m_uHi};
  uint64_t c[6];
  decimal_helper_raw_mul(a, 3, b, 3, c, 6);
  bool neg = m_bNeg ^ val.m_bNeg;
  uint8_t scale = m_uScale + val.m_uScale;
  decimal_helper_shrinkintorange(c, 6, 3, scale);
  return CFGAS_Decimal(static_cast<uint32_t>(c[0]), static_cast<uint32_t>(c[1]),
                       static_cast<uint32_t>(c[2]), neg, scale);
}

CFGAS_Decimal CFGAS_Decimal::operator/(const CFGAS_Decimal& val) const {
  if (!val.IsNotZero())
    return CFGAS_Decimal();

  bool neg = m_bNeg ^ val.m_bNeg;
  uint64_t a[7] = {m_uLo, m_uMid, m_uHi},
           b[3] = {val.m_uLo, val.m_uMid, val.m_uHi}, c[7] = {0};
  uint8_t scale = 0;
  if (m_uScale < val.m_uScale) {
    for (int i = val.m_uScale - m_uScale; i > 0; i--)
      decimal_helper_mul10_any(a, 7);
  } else {
    scale = m_uScale - val.m_uScale;
  }

  uint8_t minscale = scale;
  if (!IsNotZero())
    return CFGAS_Decimal(0, 0, 0, false, minscale);

  while (!a[6]) {
    decimal_helper_mul10_any(a, 7);
    scale++;
  }

  decimal_helper_div10_any(a, 7);
  scale--;
  decimal_helper_raw_div(a, 6, b, 3, c, 7);
  decimal_helper_shrinkintorange(c, 6, 3, scale);
  decimal_helper_truncate(c[2], c[1], c[0], scale, minscale);
  return CFGAS_Decimal(static_cast<uint32_t>(c[0]), static_cast<uint32_t>(c[1]),
                       static_cast<uint32_t>(c[2]), neg, scale);
}
