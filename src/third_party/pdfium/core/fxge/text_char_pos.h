// Copyright 2019 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_TEXT_CHAR_POS_H_
#define CORE_FXGE_TEXT_CHAR_POS_H_

#include "core/fxcrt/fx_coordinates.h"

class TextCharPos {
 public:
  TextCharPos();
  TextCharPos(const TextCharPos&);
  ~TextCharPos();

  CFX_PointF m_Origin;
  uint32_t m_Unicode = 0;
  uint32_t m_GlyphIndex = 0;
  int m_FontCharWidth = 0;
#if defined(OS_APPLE)
  uint32_t m_ExtGID = 0;
#endif
  int32_t m_FallbackFontPosition = 0;
  bool m_bGlyphAdjust = false;
  bool m_bFontStyle = false;
  float m_AdjustMatrix[4];
};

#endif  // CORE_FXGE_TEXT_CHAR_POS_H_
