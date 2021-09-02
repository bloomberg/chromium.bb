// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/layout/cfgas_break.h"

#include <algorithm>
#include <vector>

#include "core/fxcrt/fx_safe_types.h"
#include "third_party/base/stl_util.h"
#include "xfa/fgas/font/cfgas_gefont.h"

const float CFGAS_Break::kConversionFactor = 20000.0f;
const int CFGAS_Break::kMinimumTabWidth = 160000;

CFGAS_Break::CFGAS_Break(uint32_t dwLayoutStyles)
    : m_dwLayoutStyles(dwLayoutStyles), m_pCurLine(&m_Lines[0]) {}

CFGAS_Break::~CFGAS_Break() = default;

void CFGAS_Break::Reset() {
  m_eCharType = FX_CHARTYPE::kUnknown;
  for (CFGAS_BreakLine& line : m_Lines)
    line.Clear();
}

void CFGAS_Break::SetLayoutStyles(uint32_t dwLayoutStyles) {
  m_dwLayoutStyles = dwLayoutStyles;
  m_bSingleLine = (m_dwLayoutStyles & FX_LAYOUTSTYLE_SingleLine) != 0;
  m_bCombText = (m_dwLayoutStyles & FX_LAYOUTSTYLE_CombText) != 0;
}

void CFGAS_Break::SetHorizontalScale(int32_t iScale) {
  iScale = std::max(iScale, 0);
  if (m_iHorizontalScale == iScale)
    return;

  SetBreakStatus();
  m_iHorizontalScale = iScale;
}

void CFGAS_Break::SetVerticalScale(int32_t iScale) {
  if (iScale < 0)
    iScale = 0;
  if (m_iVerticalScale == iScale)
    return;

  SetBreakStatus();
  m_iVerticalScale = iScale;
}

void CFGAS_Break::SetFont(const RetainPtr<CFGAS_GEFont>& pFont) {
  if (!pFont || pFont == m_pFont)
    return;

  SetBreakStatus();
  m_pFont = pFont;
}

void CFGAS_Break::SetFontSize(float fFontSize) {
  int32_t iFontSize = FXSYS_roundf(fFontSize * 20.0f);
  if (m_iFontSize == iFontSize)
    return;

  SetBreakStatus();
  m_iFontSize = iFontSize;
}

void CFGAS_Break::SetBreakStatus() {
  ++m_dwIdentity;
  if (m_pCurLine->m_LineChars.empty())
    return;

  CFGAS_Char* tc = m_pCurLine->GetChar(m_pCurLine->m_LineChars.size() - 1);
  if (tc->m_dwStatus == CFGAS_Char::BreakType::kNone)
    tc->m_dwStatus = CFGAS_Char::BreakType::kPiece;
}

bool CFGAS_Break::IsGreaterThanLineWidth(int32_t width) const {
  FX_SAFE_INT32 line_width = m_iLineWidth;
  line_width += m_iTolerance;
  return line_width.IsValid() && width > line_width.ValueOrDie();
}

FX_CHARTYPE CFGAS_Break::GetUnifiedCharType(FX_CHARTYPE chartype) const {
  return chartype >= FX_CHARTYPE::kArabicAlef ? FX_CHARTYPE::kArabic : chartype;
}

void CFGAS_Break::SetTabWidth(float fTabWidth) {
  // Note, the use of max here was only done in the TxtBreak code. Leaving this
  // in for the RTFBreak code for consistency. If we see issues with tab widths
  // we may need to fix this.
  m_iTabWidth =
      std::max(FXSYS_roundf(fTabWidth * kConversionFactor), kMinimumTabWidth);
}

void CFGAS_Break::SetParagraphBreakChar(wchar_t wch) {
  if (wch != L'\r' && wch != L'\n')
    return;
  m_wParagraphBreakChar = wch;
}

void CFGAS_Break::SetLineBreakTolerance(float fTolerance) {
  m_iTolerance = FXSYS_roundf(fTolerance * kConversionFactor);
}

void CFGAS_Break::SetCharSpace(float fCharSpace) {
  m_iCharSpace = FXSYS_roundf(fCharSpace * kConversionFactor);
}

void CFGAS_Break::SetLineBoundary(float fLineStart, float fLineEnd) {
  if (fLineStart > fLineEnd)
    return;

  m_iLineStart = FXSYS_roundf(fLineStart * kConversionFactor);
  m_iLineWidth = FXSYS_roundf(fLineEnd * kConversionFactor);
  m_pCurLine->m_iStart = std::min(m_pCurLine->m_iStart, m_iLineWidth);
  m_pCurLine->m_iStart = std::max(m_pCurLine->m_iStart, m_iLineStart);
}

CFGAS_Char* CFGAS_Break::GetLastChar(int32_t index,
                                     bool bOmitChar,
                                     bool bRichText) const {
  std::vector<CFGAS_Char>& tca = m_pCurLine->m_LineChars;
  if (!pdfium::IndexInBounds(tca, index))
    return nullptr;

  int32_t iStart = pdfium::CollectionSize<int32_t>(tca) - 1;
  while (iStart > -1) {
    CFGAS_Char* pTC = &tca[iStart--];
    if (((bRichText && pTC->m_iCharWidth < 0) || bOmitChar) &&
        pTC->GetCharType() == FX_CHARTYPE::kCombination) {
      continue;
    }
    if (--index < 0)
      return pTC;
  }
  return nullptr;
}

int32_t CFGAS_Break::CountBreakPieces() const {
  return HasLine() ? pdfium::CollectionSize<int32_t>(
                         m_Lines[m_iReadyLineIndex].m_LinePieces)
                   : 0;
}

const CFGAS_BreakPiece* CFGAS_Break::GetBreakPieceUnstable(
    int32_t index) const {
  if (!HasLine())
    return nullptr;
  if (!pdfium::IndexInBounds(m_Lines[m_iReadyLineIndex].m_LinePieces, index))
    return nullptr;
  return &m_Lines[m_iReadyLineIndex].m_LinePieces[index];
}

void CFGAS_Break::ClearBreakPieces() {
  if (HasLine())
    m_Lines[m_iReadyLineIndex].Clear();
  m_iReadyLineIndex = -1;
}
