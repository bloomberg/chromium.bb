// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_textobject.h"

#include <algorithm>
#include <utility>

#include "core/fpdfapi/font/cpdf_cidfont.h"
#include "core/fpdfapi/font/cpdf_font.h"

#define ISLATINWORD(u) (u != 0x20 && u <= 0x28FF)

CPDF_TextObjectItem::CPDF_TextObjectItem() : m_CharCode(0) {}

CPDF_TextObjectItem::~CPDF_TextObjectItem() = default;

CPDF_TextObject::CPDF_TextObject(int32_t content_stream)
    : CPDF_PageObject(content_stream) {}

CPDF_TextObject::CPDF_TextObject() : CPDF_TextObject(kNoContentStream) {}

CPDF_TextObject::~CPDF_TextObject() {
  // Move m_CharCodes to a local variable so it will be captured in crash dumps,
  // to help with investigating crbug.com/782215.
  auto char_codes_copy = std::move(m_CharCodes);
}

size_t CPDF_TextObject::CountItems() const {
  return m_CharCodes.size();
}

void CPDF_TextObject::GetItemInfo(size_t index,
                                  CPDF_TextObjectItem* pInfo) const {
  ASSERT(index < m_CharCodes.size());
  pInfo->m_CharCode = m_CharCodes[index];
  pInfo->m_Origin = CFX_PointF(index > 0 ? m_CharPos[index - 1] : 0, 0);
  if (pInfo->m_CharCode == CPDF_Font::kInvalidCharCode)
    return;

  RetainPtr<CPDF_Font> pFont = GetFont();
  if (!pFont->IsCIDFont() || !pFont->AsCIDFont()->IsVertWriting())
    return;

  uint16_t cid = pFont->AsCIDFont()->CIDFromCharCode(pInfo->m_CharCode);
  pInfo->m_Origin = CFX_PointF(0, pInfo->m_Origin.x);

  CFX_Point16 vertical_origin = pFont->AsCIDFont()->GetVertOrigin(cid);
  float fontsize = GetFontSize();
  pInfo->m_Origin.x -= fontsize * vertical_origin.x / 1000;
  pInfo->m_Origin.y -= fontsize * vertical_origin.y / 1000;
}

size_t CPDF_TextObject::CountChars() const {
  size_t count = 0;
  for (uint32_t charcode : m_CharCodes) {
    if (charcode != CPDF_Font::kInvalidCharCode)
      ++count;
  }
  return count;
}

uint32_t CPDF_TextObject::GetCharCode(size_t index) const {
  size_t count = 0;
  for (uint32_t code : m_CharCodes) {
    if (code == CPDF_Font::kInvalidCharCode)
      continue;
    if (count++ != index)
      continue;
    return code;
  }
  return CPDF_Font::kInvalidCharCode;
}

void CPDF_TextObject::GetCharInfo(size_t index,
                                  CPDF_TextObjectItem* pInfo) const {
  size_t count = 0;
  for (size_t i = 0; i < m_CharCodes.size(); ++i) {
    uint32_t charcode = m_CharCodes[i];
    if (charcode == CPDF_Font::kInvalidCharCode)
      continue;
    if (count++ != index)
      continue;
    GetItemInfo(i, pInfo);
    break;
  }
}

int CPDF_TextObject::CountWords() const {
  RetainPtr<CPDF_Font> pFont = GetFont();
  bool bInLatinWord = false;
  int nWords = 0;
  for (size_t i = 0, sz = CountChars(); i < sz; ++i) {
    uint32_t charcode = GetCharCode(i);

    WideString swUnicode = pFont->UnicodeFromCharCode(charcode);
    uint16_t unicode = 0;
    if (swUnicode.GetLength() > 0)
      unicode = swUnicode[0];

    bool bIsLatin = ISLATINWORD(unicode);
    if (bIsLatin && bInLatinWord)
      continue;

    bInLatinWord = bIsLatin;
    if (unicode != 0x20)
      nWords++;
  }

  return nWords;
}

WideString CPDF_TextObject::GetWordString(int nWordIndex) const {
  RetainPtr<CPDF_Font> pFont = GetFont();
  WideString swRet;
  int nWords = 0;
  bool bInLatinWord = false;
  for (size_t i = 0, sz = CountChars(); i < sz; ++i) {
    uint32_t charcode = GetCharCode(i);

    WideString swUnicode = pFont->UnicodeFromCharCode(charcode);
    uint16_t unicode = 0;
    if (swUnicode.GetLength() > 0)
      unicode = swUnicode[0];

    bool bIsLatin = ISLATINWORD(unicode);
    if (!bIsLatin || !bInLatinWord) {
      bInLatinWord = bIsLatin;
      if (unicode != 0x20)
        nWords++;
    }
    if (nWords - 1 == nWordIndex)
      swRet += unicode;
  }
  return swRet;
}

std::unique_ptr<CPDF_TextObject> CPDF_TextObject::Clone() const {
  auto obj = std::make_unique<CPDF_TextObject>();
  obj->CopyData(this);
  obj->m_CharCodes = m_CharCodes;
  obj->m_CharPos = m_CharPos;
  obj->m_Pos = m_Pos;
  return obj;
}

CPDF_PageObject::Type CPDF_TextObject::GetType() const {
  return TEXT;
}

void CPDF_TextObject::Transform(const CFX_Matrix& matrix) {
  CFX_Matrix text_matrix = GetTextMatrix() * matrix;

  float* pTextMatrix = m_TextState.GetMutableMatrix();
  pTextMatrix[0] = text_matrix.a;
  pTextMatrix[1] = text_matrix.c;
  pTextMatrix[2] = text_matrix.b;
  pTextMatrix[3] = text_matrix.d;
  m_Pos = CFX_PointF(text_matrix.e, text_matrix.f);
  CalcPositionData(0);
  SetDirty(true);
}

bool CPDF_TextObject::IsText() const {
  return true;
}

CPDF_TextObject* CPDF_TextObject::AsText() {
  return this;
}

const CPDF_TextObject* CPDF_TextObject::AsText() const {
  return this;
}

CFX_Matrix CPDF_TextObject::GetTextMatrix() const {
  const float* pTextMatrix = m_TextState.GetMatrix();
  return CFX_Matrix(pTextMatrix[0], pTextMatrix[2], pTextMatrix[1],
                    pTextMatrix[3], m_Pos.x, m_Pos.y);
}

void CPDF_TextObject::SetSegments(const ByteString* pStrs,
                                  const std::vector<float>& kernings,
                                  size_t nSegs) {
  m_CharCodes.clear();
  m_CharPos.clear();
  RetainPtr<CPDF_Font> pFont = GetFont();
  int nChars = 0;
  for (size_t i = 0; i < nSegs; ++i)
    nChars += pFont->CountChar(pStrs[i].AsStringView());
  nChars += nSegs - 1;
  m_CharCodes.resize(nChars);
  m_CharPos.resize(nChars - 1);
  size_t index = 0;
  for (size_t i = 0; i < nSegs; ++i) {
    ByteStringView segment = pStrs[i].AsStringView();
    size_t offset = 0;
    while (offset < segment.GetLength()) {
      ASSERT(index < m_CharCodes.size());
      m_CharCodes[index++] = pFont->GetNextChar(segment, &offset);
    }
    if (i != nSegs - 1) {
      m_CharPos[index - 1] = kernings[i];
      m_CharCodes[index++] = CPDF_Font::kInvalidCharCode;
    }
  }
}

void CPDF_TextObject::SetText(const ByteString& str) {
  SetSegments(&str, std::vector<float>(), 1);
  RecalcPositionData();
  SetDirty(true);
}

float CPDF_TextObject::GetCharWidth(uint32_t charcode) const {
  float fontsize = GetFontSize() / 1000;
  RetainPtr<CPDF_Font> pFont = GetFont();
  bool bVertWriting = false;
  CPDF_CIDFont* pCIDFont = pFont->AsCIDFont();
  if (pCIDFont)
    bVertWriting = pCIDFont->IsVertWriting();
  if (!bVertWriting)
    return pFont->GetCharWidthF(charcode) * fontsize;

  uint16_t cid = pCIDFont->CIDFromCharCode(charcode);
  return pCIDFont->GetVertWidth(cid) * fontsize;
}

RetainPtr<CPDF_Font> CPDF_TextObject::GetFont() const {
  return m_TextState.GetFont();
}

float CPDF_TextObject::GetFontSize() const {
  return m_TextState.GetFontSize();
}

TextRenderingMode CPDF_TextObject::GetTextRenderMode() const {
  return m_TextState.GetTextMode();
}

void CPDF_TextObject::SetTextRenderMode(TextRenderingMode mode) {
  m_TextState.SetTextMode(mode);
  SetDirty(true);
}

CFX_PointF CPDF_TextObject::CalcPositionData(float horz_scale) {
  float curpos = 0;
  float min_x = 10000 * 1.0f;
  float max_x = -10000 * 1.0f;
  float min_y = 10000 * 1.0f;
  float max_y = -10000 * 1.0f;
  RetainPtr<CPDF_Font> pFont = GetFont();
  bool bVertWriting = false;
  CPDF_CIDFont* pCIDFont = pFont->AsCIDFont();
  if (pCIDFont)
    bVertWriting = pCIDFont->IsVertWriting();

  float fontsize = GetFontSize();
  for (size_t i = 0; i < m_CharCodes.size(); ++i) {
    uint32_t charcode = m_CharCodes[i];
    if (i > 0) {
      if (charcode == CPDF_Font::kInvalidCharCode) {
        curpos -= (m_CharPos[i - 1] * fontsize) / 1000;
        continue;
      }
      m_CharPos[i - 1] = curpos;
    }

    FX_RECT char_rect = pFont->GetCharBBox(charcode);
    float charwidth;
    if (!bVertWriting) {
      min_y = std::min(
          min_y, static_cast<float>(std::min(char_rect.top, char_rect.bottom)));
      max_y = std::max(
          max_y, static_cast<float>(std::max(char_rect.top, char_rect.bottom)));
      float char_left = curpos + char_rect.left * fontsize / 1000;
      float char_right = curpos + char_rect.right * fontsize / 1000;
      min_x = std::min(min_x, std::min(char_left, char_right));
      max_x = std::max(max_x, std::max(char_left, char_right));
      charwidth = pFont->GetCharWidthF(charcode) * fontsize / 1000;
    } else {
      uint16_t cid = pCIDFont->CIDFromCharCode(charcode);
      CFX_Point16 vertical_origin = pCIDFont->GetVertOrigin(cid);
      char_rect.left -= vertical_origin.x;
      char_rect.right -= vertical_origin.x;
      char_rect.top -= vertical_origin.y;
      char_rect.bottom -= vertical_origin.y;
      min_x = std::min(
          min_x, static_cast<float>(std::min(char_rect.left, char_rect.right)));
      max_x = std::max(
          max_x, static_cast<float>(std::max(char_rect.left, char_rect.right)));
      float char_top = curpos + char_rect.top * fontsize / 1000;
      float char_bottom = curpos + char_rect.bottom * fontsize / 1000;
      min_y = std::min(min_y, std::min(char_top, char_bottom));
      max_y = std::max(max_y, std::max(char_top, char_bottom));
      charwidth = pCIDFont->GetVertWidth(cid) * fontsize / 1000;
    }
    curpos += charwidth;
    if (charcode == ' ' && (!pCIDFont || pCIDFont->GetCharSize(' ') == 1))
      curpos += m_TextState.GetWordSpace();

    curpos += m_TextState.GetCharSpace();
  }

  CFX_PointF ret;
  if (bVertWriting) {
    ret.y = curpos;
    min_x = min_x * fontsize / 1000;
    max_x = max_x * fontsize / 1000;
  } else {
    ret.x = curpos * horz_scale;
    min_y = min_y * fontsize / 1000;
    max_y = max_y * fontsize / 1000;
  }
  SetRect(
      GetTextMatrix().TransformRect(CFX_FloatRect(min_x, min_y, max_x, max_y)));

  if (!TextRenderingModeIsStrokeMode(m_TextState.GetTextMode()))
    return ret;

  float half_width = m_GraphState.GetLineWidth() / 2;
  m_Rect.left -= half_width;
  m_Rect.right += half_width;
  m_Rect.top += half_width;
  m_Rect.bottom -= half_width;

  return ret;
}

void CPDF_TextObject::RecalcPositionData() {
  CalcPositionData(1);
}
