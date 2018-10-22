// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_STRING_H_
#define CORE_FXCRT_FX_STRING_H_

#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/widestring.h"

#define FXBSTR_ID(c1, c2, c3, c4)                                      \
  (((uint32_t)c1 << 24) | ((uint32_t)c2 << 16) | ((uint32_t)c3 << 8) | \
   ((uint32_t)c4))

ByteString FX_UTF8Encode(const WideStringView& wsStr);
WideString FX_UTF8Decode(const ByteStringView& bsStr);

float FX_atof(const ByteStringView& str);
float FX_atof(const WideStringView& wsStr);
bool FX_atonum(const ByteStringView& str, void* pData);
size_t FX_ftoa(float f, char* buf);

#endif  // CORE_FXCRT_FX_STRING_H_
