// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_textlayout.h"

#include <algorithm>
#include <utility>

#include "core/fxcrt/css/cfx_csscomputedstyle.h"
#include "core/fxcrt/css/cfx_cssstyleselector.h"
#include "core/fxcrt/xml/cfx_xmlelement.h"
#include "core/fxcrt/xml/cfx_xmlnode.h"
#include "core/fxcrt/xml/cfx_xmltext.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/text_char_pos.h"
#include "fxjs/xfa/cjx_object.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fde/cfde_textout.h"
#include "xfa/fgas/font/cfgas_gefont.h"
#include "xfa/fgas/layout/cfx_linkuserdata.h"
#include "xfa/fgas/layout/cfx_rtfbreak.h"
#include "xfa/fgas/layout/cfx_textuserdata.h"
#include "xfa/fxfa/cxfa_loadercontext.h"
#include "xfa/fxfa/cxfa_pieceline.h"
#include "xfa/fxfa/cxfa_textparsecontext.h"
#include "xfa/fxfa/cxfa_textpiece.h"
#include "xfa/fxfa/cxfa_textprovider.h"
#include "xfa/fxfa/cxfa_texttabstopscontext.h"
#include "xfa/fxfa/parser/cxfa_font.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_para.h"

namespace {

constexpr float kHeightTolerance = 0.001f;

void ProcessText(WideString* pText) {
  int32_t iLen = pText->GetLength();
  if (iLen == 0)
    return;

  int32_t iTrimLeft = 0;
  {
    // Span's lifetime must end before ReleaseBuffer() below.
    pdfium::span<wchar_t> psz = pText->GetBuffer(iLen);
    wchar_t wPrev = 0;
    for (int32_t i = 0; i < iLen; i++) {
      wchar_t wch = psz[i];
      if (wch < 0x20)
        wch = 0x20;
      if (wch == 0x20 && wPrev == 0x20)
        continue;

      wPrev = wch;
      psz[iTrimLeft++] = wch;
    }
  }
  pText->ReleaseBuffer(iTrimLeft);
}

}  // namespace

CXFA_TextLayout::CXFA_TextLayout(CXFA_FFDoc* doc,
                                 CXFA_TextProvider* pTextProvider)
    : m_pDoc(doc), m_pTextProvider(pTextProvider) {
  ASSERT(m_pTextProvider);
}

CXFA_TextLayout::~CXFA_TextLayout() {
  m_textParser.Reset();
  Unload();
}

void CXFA_TextLayout::Unload() {
  m_pieceLines.clear();
  m_pBreak.reset();
}

void CXFA_TextLayout::GetTextDataNode() {
  CXFA_Node* pNode = m_pTextProvider->GetTextNode(&m_bRichText);
  if (pNode && m_bRichText)
    m_textParser.Reset();

  m_pTextDataNode = pNode;
}

CFX_XMLNode* CXFA_TextLayout::GetXMLContainerNode() {
  if (!m_bRichText)
    return nullptr;

  CFX_XMLNode* pXMLRoot = m_pTextDataNode->GetXMLMappingNode();
  if (!pXMLRoot)
    return nullptr;

  for (CFX_XMLNode* pXMLChild = pXMLRoot->GetFirstChild(); pXMLChild;
       pXMLChild = pXMLChild->GetNextSibling()) {
    CFX_XMLElement* pXMLElement = ToXMLElement(pXMLChild);
    if (!pXMLElement)
      continue;
    WideString wsTag = pXMLElement->GetLocalTagName();
    if (wsTag.EqualsASCII("body") || wsTag.EqualsASCII("html"))
      return pXMLChild;
  }
  return nullptr;
}

std::unique_ptr<CFX_RTFBreak> CXFA_TextLayout::CreateBreak(bool bDefault) {
  uint32_t dwStyle = FX_LAYOUTSTYLE_ExpandTab;
  if (!bDefault)
    dwStyle |= FX_LAYOUTSTYLE_Pagination;

  auto pBreak = pdfium::MakeUnique<CFX_RTFBreak>(dwStyle);
  pBreak->SetLineBreakTolerance(1);
  pBreak->SetFont(m_textParser.GetFont(m_pDoc.Get(), m_pTextProvider, nullptr));
  pBreak->SetFontSize(m_textParser.GetFontSize(m_pTextProvider, nullptr));
  return pBreak;
}

void CXFA_TextLayout::InitBreak(float fLineWidth) {
  CXFA_Para* para = m_pTextProvider->GetParaIfExists();
  float fStart = 0;
  float fStartPos = 0;
  if (para) {
    CFX_RTFLineAlignment iAlign = CFX_RTFLineAlignment::Left;
    switch (para->GetHorizontalAlign()) {
      case XFA_AttributeValue::Center:
        iAlign = CFX_RTFLineAlignment::Center;
        break;
      case XFA_AttributeValue::Right:
        iAlign = CFX_RTFLineAlignment::Right;
        break;
      case XFA_AttributeValue::Justify:
        iAlign = CFX_RTFLineAlignment::Justified;
        break;
      case XFA_AttributeValue::JustifyAll:
        iAlign = CFX_RTFLineAlignment::Distributed;
        break;
      case XFA_AttributeValue::Left:
      case XFA_AttributeValue::Radix:
        break;
      default:
        NOTREACHED();
        break;
    }
    m_pBreak->SetAlignment(iAlign);

    fStart = para->GetMarginLeft();
    if (m_pTextProvider->IsCheckButtonAndAutoWidth()) {
      if (iAlign != CFX_RTFLineAlignment::Left)
        fLineWidth -= para->GetMarginRight();
    } else {
      fLineWidth -= para->GetMarginRight();
    }
    if (fLineWidth < 0)
      fLineWidth = fStart;

    fStartPos = fStart;
    float fIndent = para->GetTextIndent();
    if (fIndent > 0)
      fStartPos += fIndent;
  }

  m_pBreak->SetLineBoundary(fStart, fLineWidth);
  m_pBreak->SetLineStartPos(fStartPos);

  CXFA_Font* font = m_pTextProvider->GetFontIfExists();
  if (font) {
    m_pBreak->SetHorizontalScale(
        static_cast<int32_t>(font->GetHorizontalScale()));
    m_pBreak->SetVerticalScale(static_cast<int32_t>(font->GetVerticalScale()));
    m_pBreak->SetCharSpace(font->GetLetterSpacing());
  }

  float fFontSize = m_textParser.GetFontSize(m_pTextProvider, nullptr);
  m_pBreak->SetFontSize(fFontSize);
  m_pBreak->SetFont(
      m_textParser.GetFont(m_pDoc.Get(), m_pTextProvider, nullptr));
  m_pBreak->SetLineBreakTolerance(fFontSize * 0.2f);
}

void CXFA_TextLayout::InitBreak(CFX_CSSComputedStyle* pStyle,
                                CFX_CSSDisplay eDisplay,
                                float fLineWidth,
                                const CFX_XMLNode* pXMLNode,
                                CFX_CSSComputedStyle* pParentStyle) {
  if (!pStyle) {
    InitBreak(fLineWidth);
    return;
  }

  if (eDisplay == CFX_CSSDisplay::Block ||
      eDisplay == CFX_CSSDisplay::ListItem) {
    CFX_RTFLineAlignment iAlign = CFX_RTFLineAlignment::Left;
    switch (pStyle->GetTextAlign()) {
      case CFX_CSSTextAlign::Right:
        iAlign = CFX_RTFLineAlignment::Right;
        break;
      case CFX_CSSTextAlign::Center:
        iAlign = CFX_RTFLineAlignment::Center;
        break;
      case CFX_CSSTextAlign::Justify:
        iAlign = CFX_RTFLineAlignment::Justified;
        break;
      case CFX_CSSTextAlign::JustifyAll:
        iAlign = CFX_RTFLineAlignment::Distributed;
        break;
      default:
        break;
    }
    m_pBreak->SetAlignment(iAlign);

    float fStart = 0;
    const CFX_CSSRect* pRect = pStyle->GetMarginWidth();
    const CFX_CSSRect* pPaddingRect = pStyle->GetPaddingWidth();
    if (pRect) {
      fStart = pRect->left.GetValue();
      fLineWidth -= pRect->right.GetValue();
      if (pPaddingRect) {
        fStart += pPaddingRect->left.GetValue();
        fLineWidth -= pPaddingRect->right.GetValue();
      }
      if (eDisplay == CFX_CSSDisplay::ListItem) {
        const CFX_CSSRect* pParRect = pParentStyle->GetMarginWidth();
        const CFX_CSSRect* pParPaddingRect = pParentStyle->GetPaddingWidth();
        if (pParRect) {
          fStart += pParRect->left.GetValue();
          fLineWidth -= pParRect->right.GetValue();
          if (pParPaddingRect) {
            fStart += pParPaddingRect->left.GetValue();
            fLineWidth -= pParPaddingRect->right.GetValue();
          }
        }
        CFX_CSSRect pNewRect;
        pNewRect.left.Set(CFX_CSSLengthUnit::Point, fStart);
        pNewRect.right.Set(CFX_CSSLengthUnit::Point, pRect->right.GetValue());
        pNewRect.top.Set(CFX_CSSLengthUnit::Point, pRect->top.GetValue());
        pNewRect.bottom.Set(CFX_CSSLengthUnit::Point, pRect->bottom.GetValue());
        pStyle->SetMarginWidth(pNewRect);
      }
    }
    m_pBreak->SetLineBoundary(fStart, fLineWidth);
    float fIndent = pStyle->GetTextIndent().GetValue();
    if (fIndent > 0)
      fStart += fIndent;

    m_pBreak->SetLineStartPos(fStart);
    m_pBreak->SetTabWidth(m_textParser.GetTabInterval(pStyle));
    if (!m_pTabstopContext)
      m_pTabstopContext = pdfium::MakeUnique<CXFA_TextTabstopsContext>();
    m_textParser.GetTabstops(pStyle, m_pTabstopContext.get());
    for (const auto& stop : m_pTabstopContext->m_tabstops)
      m_pBreak->AddPositionedTab(stop.fTabstops);
  }
  float fFontSize = m_textParser.GetFontSize(m_pTextProvider, pStyle);
  m_pBreak->SetFontSize(fFontSize);
  m_pBreak->SetLineBreakTolerance(fFontSize * 0.2f);
  m_pBreak->SetFont(
      m_textParser.GetFont(m_pDoc.Get(), m_pTextProvider, pStyle));
  m_pBreak->SetHorizontalScale(
      m_textParser.GetHorScale(m_pTextProvider, pStyle, pXMLNode));
  m_pBreak->SetVerticalScale(m_textParser.GetVerScale(m_pTextProvider, pStyle));
  m_pBreak->SetCharSpace(pStyle->GetLetterSpacing().GetValue());
}

float CXFA_TextLayout::GetLayoutHeight() {
  if (!m_pLoader)
    return 0;

  if (m_pLoader->lineHeights.empty() && m_pLoader->fWidth > 0) {
    CFX_SizeF szMax(m_pLoader->fWidth, m_pLoader->fHeight);
    m_pLoader->bSaveLineHeight = true;
    m_pLoader->fLastPos = 0;
    CFX_SizeF szDef = CalcSize(szMax, szMax);
    m_pLoader->bSaveLineHeight = false;
    return szDef.height;
  }

  float fHeight = m_pLoader->fHeight;
  if (fHeight < 0.1f) {
    fHeight = 0;
    for (float value : m_pLoader->lineHeights)
      fHeight += value;
  }
  return fHeight;
}

float CXFA_TextLayout::StartLayout(float fWidth) {
  if (!m_pLoader)
    m_pLoader = pdfium::MakeUnique<CXFA_LoaderContext>();

  if (fWidth < 0 ||
      (m_pLoader->fWidth > -1 && fabs(fWidth - m_pLoader->fWidth) > 0)) {
    m_pLoader->lineHeights.clear();
    m_Blocks.clear();
    Unload();
    m_pLoader->fStartLineOffset = 0;
  }
  m_pLoader->fWidth = fWidth;

  if (fWidth >= 0)
    return fWidth;

  CFX_SizeF szMax;

  m_pLoader->bSaveLineHeight = true;
  m_pLoader->fLastPos = 0;
  CFX_SizeF szDef = CalcSize(szMax, szMax);
  m_pLoader->bSaveLineHeight = false;
  return szDef.width;
}

float CXFA_TextLayout::DoLayout(float fTextHeight) {
  if (!m_pLoader)
    return fTextHeight;

  UpdateLoaderHeight(fTextHeight);
  return fTextHeight;
}

float CXFA_TextLayout::DoSplitLayout(size_t szBlockIndex,
                                     float fCalcHeight,
                                     float fTextHeight) {
  if (!m_pLoader)
    return fCalcHeight;

  UpdateLoaderHeight(fTextHeight);

  if (fCalcHeight < 0)
    return fCalcHeight;

  m_bHasBlock = true;
  if (m_Blocks.empty() && m_pLoader->fHeight > 0) {
    float fHeight = fTextHeight - GetLayoutHeight();
    if (fHeight > 0) {
      XFA_AttributeValue iAlign = m_textParser.GetVAlign(m_pTextProvider);
      if (iAlign == XFA_AttributeValue::Middle)
        fHeight /= 2.0f;
      else if (iAlign != XFA_AttributeValue::Bottom)
        fHeight = 0;
      m_pLoader->fStartLineOffset = fHeight;
    }
  }

  float fLinePos = m_pLoader->fStartLineOffset;
  size_t szLineIndex = 0;
  if (!m_Blocks.empty()) {
    if (szBlockIndex < m_Blocks.size())
      szLineIndex = m_Blocks[szBlockIndex].szIndex;
    else
      szLineIndex = GetNextIndexFromLastBlockData();
    for (size_t i = 0;
         i < std::min(szBlockIndex, m_pLoader->blockHeights.size()); ++i) {
      fLinePos -= m_pLoader->blockHeights[i].fHeight;
    }
  }

  if (szLineIndex >= m_pLoader->lineHeights.size())
    return fCalcHeight;

  if (m_pLoader->lineHeights[szLineIndex] - fCalcHeight > kHeightTolerance)
    return 0;

  for (size_t i = szLineIndex; i < m_pLoader->lineHeights.size(); ++i) {
    float fLineHeight = m_pLoader->lineHeights[i];
    if (fLinePos + fLineHeight - fCalcHeight <= kHeightTolerance) {
      fLinePos += fLineHeight;
      continue;
    }

    if (szBlockIndex < m_Blocks.size())
      m_Blocks[szBlockIndex] = {szLineIndex, i - szLineIndex};
    else
      m_Blocks.push_back({szLineIndex, i - szLineIndex});

    if (i != szLineIndex)
      return fLinePos;

    if (fCalcHeight > fLinePos)
      return fCalcHeight;

    if (szBlockIndex < m_pLoader->blockHeights.size() &&
        m_pLoader->blockHeights[szBlockIndex].szBlockIndex == szBlockIndex) {
      m_pLoader->blockHeights[szBlockIndex].fHeight = fCalcHeight;
    } else {
      m_pLoader->blockHeights.push_back({szBlockIndex, fCalcHeight});
    }
    return fCalcHeight;
  }
  return fCalcHeight;
}

size_t CXFA_TextLayout::CountBlocks() const {
  size_t szCount = m_Blocks.size();
  return szCount > 0 ? szCount : 1;
}

size_t CXFA_TextLayout::GetNextIndexFromLastBlockData() const {
  return m_Blocks.back().szIndex + m_Blocks.back().szLength;
}

void CXFA_TextLayout::UpdateLoaderHeight(float fTextHeight) {
  m_pLoader->fHeight = fTextHeight;
  if (m_pLoader->fHeight < 0)
    m_pLoader->fHeight = GetLayoutHeight();
}

CFX_SizeF CXFA_TextLayout::CalcSize(const CFX_SizeF& minSize,
                                    const CFX_SizeF& maxSize) {
  float width = maxSize.width;
  if (width < 1)
    width = 0xFFFF;

  m_pBreak = CreateBreak(false);
  float fLinePos = 0;
  m_iLines = 0;
  m_fMaxWidth = 0;
  Loader(width, &fLinePos, false);
  if (fLinePos < 0.1f)
    fLinePos = m_textParser.GetFontSize(m_pTextProvider, nullptr);

  m_pTabstopContext.reset();
  return CFX_SizeF(m_fMaxWidth, fLinePos);
}

float CXFA_TextLayout::Layout(const CFX_SizeF& size) {
  if (size.width < 1)
    return 0.f;

  Unload();
  m_pBreak = CreateBreak(true);
  if (m_pLoader) {
    m_pLoader->iTotalLines = -1;
    m_pLoader->iChar = 0;
  }

  m_iLines = 0;
  float fLinePos = 0;
  Loader(size.width, &fLinePos, true);
  UpdateAlign(size.height, fLinePos);
  m_pTabstopContext.reset();
  return fLinePos;
}

bool CXFA_TextLayout::LayoutInternal(size_t szBlockIndex) {
  ASSERT(szBlockIndex < CountBlocks());

  if (!m_pLoader || m_pLoader->fWidth < 1)
    return false;

  m_pLoader->iTotalLines = -1;
  m_iLines = 0;
  float fLinePos = 0;
  CXFA_Node* pNode = nullptr;
  CFX_SizeF szText(m_pLoader->fWidth, m_pLoader->fHeight);
  if (szBlockIndex < m_pLoader->blockHeights.size())
    return true;
  if (szBlockIndex == m_pLoader->blockHeights.size()) {
    Unload();
    m_pBreak = CreateBreak(true);
    fLinePos = m_pLoader->fStartLineOffset;
    for (size_t i = 0; i < m_pLoader->blockHeights.size(); ++i)
      fLinePos -= m_pLoader->blockHeights[i].fHeight;

    m_pLoader->iChar = 0;
    if (!m_Blocks.empty())
      m_pLoader->iTotalLines = m_Blocks[szBlockIndex].szLength;

    Loader(szText.width, &fLinePos, true);
    if (m_Blocks.empty() && m_pLoader->fStartLineOffset < 0.1f)
      UpdateAlign(szText.height, fLinePos);
  } else if (m_pTextDataNode) {
    if (!m_Blocks.empty() && szBlockIndex < m_Blocks.size() - 1)
      m_pLoader->iTotalLines = m_Blocks[szBlockIndex].szLength;

    m_pBreak->Reset();
    if (m_bRichText) {
      CFX_XMLNode* pContainerNode = GetXMLContainerNode();
      if (!pContainerNode)
        return true;

      const CFX_XMLNode* pXMLNode = m_pLoader->pXMLNode.Get();
      if (!pXMLNode)
        return true;

      const CFX_XMLNode* pSaveXMLNode = pXMLNode;
      for (; pXMLNode; pXMLNode = pXMLNode->GetNextSibling()) {
        if (!LoadRichText(pXMLNode, szText.width, &fLinePos,
                          m_pLoader->pParentStyle, true, nullptr, true, false,
                          0)) {
          break;
        }
      }
      while (!pXMLNode) {
        pXMLNode = pSaveXMLNode->GetParent();
        if (pXMLNode == pContainerNode)
          break;
        if (!LoadRichText(pXMLNode, szText.width, &fLinePos,
                          m_pLoader->pParentStyle, true, nullptr, false, false,
                          0)) {
          break;
        }
        pSaveXMLNode = pXMLNode;
        pXMLNode = pXMLNode->GetNextSibling();
        if (!pXMLNode)
          continue;
        for (; pXMLNode; pXMLNode = pXMLNode->GetNextSibling()) {
          if (!LoadRichText(pXMLNode, szText.width, &fLinePos,
                            m_pLoader->pParentStyle, true, nullptr, true, false,
                            0)) {
            break;
          }
        }
      }
    } else {
      pNode = m_pLoader->pNode.Get();
      if (!pNode)
        return true;
      LoadText(pNode, szText.width, &fLinePos, true);
    }
  }
  if (szBlockIndex == m_Blocks.size()) {
    m_pTabstopContext.reset();
    m_pLoader.reset();
  }
  return true;
}

void CXFA_TextLayout::ItemBlocks(const CFX_RectF& rtText, size_t szBlockIndex) {
  if (!m_pLoader)
    return;

  if (m_pLoader->lineHeights.empty())
    return;

  float fLinePos = m_pLoader->fStartLineOffset;
  size_t szLineIndex = 0;
  if (szBlockIndex > 0) {
    if (szBlockIndex <= m_pLoader->blockHeights.size()) {
      for (size_t i = 0; i < szBlockIndex; ++i)
        fLinePos -= m_pLoader->blockHeights[i].fHeight;
    } else {
      fLinePos = 0;
    }
    szLineIndex = GetNextIndexFromLastBlockData();
  }

  size_t i;
  for (i = szLineIndex; i < m_pLoader->lineHeights.size(); ++i) {
    float fLineHeight = m_pLoader->lineHeights[i];
    if (fLinePos + fLineHeight - rtText.height > kHeightTolerance) {
      m_Blocks.push_back({szLineIndex, i - szLineIndex});
      return;
    }
    fLinePos += fLineHeight;
  }
  if (i > szLineIndex)
    m_Blocks.push_back({szLineIndex, i - szLineIndex});
}

bool CXFA_TextLayout::DrawString(CFX_RenderDevice* pFxDevice,
                                 const CFX_Matrix& mtDoc2Device,
                                 const CFX_RectF& rtClip,
                                 size_t szBlockIndex) {
  if (!pFxDevice)
    return false;

  pFxDevice->SaveState();
  pFxDevice->SetClip_Rect(rtClip.GetOuterRect());

  if (m_pieceLines.empty()) {
    size_t szBlockCount = CountBlocks();
    for (size_t i = 0; i < szBlockCount; ++i)
      LayoutInternal(i);
  }

  std::vector<TextCharPos> char_pos(1);
  size_t szLineStart = 0;
  size_t szPieceLines = m_pieceLines.size();
  if (!m_Blocks.empty()) {
    if (szBlockIndex < m_Blocks.size()) {
      szLineStart = m_Blocks[szBlockIndex].szIndex;
      szPieceLines = m_Blocks[szBlockIndex].szLength;
    } else {
      szPieceLines = 0;
    }
  }

  for (size_t i = 0; i < szPieceLines; ++i) {
    if (i + szLineStart >= m_pieceLines.size())
      break;

    CXFA_PieceLine* pPieceLine = m_pieceLines[i + szLineStart].get();
    for (size_t j = 0; j < pPieceLine->m_textPieces.size(); ++j) {
      const CXFA_TextPiece* pPiece = pPieceLine->m_textPieces[j].get();
      int32_t iChars = pPiece->iChars;
      if (pdfium::CollectionSize<int32_t>(char_pos) < iChars)
        char_pos.resize(iChars);
      RenderString(pFxDevice, pPieceLine, j, &char_pos, mtDoc2Device);
    }
    for (size_t j = 0; j < pPieceLine->m_textPieces.size(); ++j)
      RenderPath(pFxDevice, pPieceLine, j, &char_pos, mtDoc2Device);
  }
  pFxDevice->RestoreState(false);
  return szPieceLines > 0;
}

void CXFA_TextLayout::UpdateAlign(float fHeight, float fBottom) {
  fHeight -= fBottom;
  if (fHeight < 0.1f)
    return;

  switch (m_textParser.GetVAlign(m_pTextProvider)) {
    case XFA_AttributeValue::Middle:
      fHeight /= 2.0f;
      break;
    case XFA_AttributeValue::Bottom:
      break;
    default:
      return;
  }

  for (const auto& pPieceLine : m_pieceLines) {
    for (const auto& pPiece : pPieceLine->m_textPieces)
      pPiece->rtPiece.top += fHeight;
  }
}

void CXFA_TextLayout::Loader(float textWidth,
                             float* pLinePos,
                             bool bSavePieces) {
  GetTextDataNode();
  if (!m_pTextDataNode)
    return;

  if (!m_bRichText) {
    LoadText(m_pTextDataNode, textWidth, pLinePos, bSavePieces);
    return;
  }

  const CFX_XMLNode* pXMLContainer = GetXMLContainerNode();
  if (!pXMLContainer)
    return;

  if (!m_textParser.IsParsed())
    m_textParser.DoParse(pXMLContainer, m_pTextProvider);

  auto pRootStyle = m_textParser.CreateRootStyle(m_pTextProvider);
  LoadRichText(pXMLContainer, textWidth, pLinePos, pRootStyle, bSavePieces,
               nullptr, true, false, 0);
}

void CXFA_TextLayout::LoadText(CXFA_Node* pNode,
                               float textWidth,
                               float* pLinePos,
                               bool bSavePieces) {
  InitBreak(textWidth);

  CXFA_Para* para = m_pTextProvider->GetParaIfExists();
  float fSpaceAbove = 0;
  if (para) {
    fSpaceAbove = para->GetSpaceAbove();
    if (fSpaceAbove < 0.1f)
      fSpaceAbove = 0;

    switch (para->GetVerticalAlign()) {
      case XFA_AttributeValue::Top:
      case XFA_AttributeValue::Middle:
      case XFA_AttributeValue::Bottom: {
        *pLinePos += fSpaceAbove;
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  WideString wsText = pNode->JSObject()->GetContent(false);
  wsText.TrimRight(L" ");
  bool bRet = AppendChar(wsText, pLinePos, fSpaceAbove, bSavePieces);
  if (bRet && m_pLoader)
    m_pLoader->pNode = pNode;
  else
    EndBreak(CFX_BreakType::Paragraph, pLinePos, bSavePieces);
}

bool CXFA_TextLayout::LoadRichText(
    const CFX_XMLNode* pXMLNode,
    float textWidth,
    float* pLinePos,
    const RetainPtr<CFX_CSSComputedStyle>& pParentStyle,
    bool bSavePieces,
    RetainPtr<CFX_LinkUserData> pLinkData,
    bool bEndBreak,
    bool bIsOl,
    int32_t iLiCount) {
  if (!pXMLNode)
    return false;

  CXFA_TextParseContext* pContext =
      m_textParser.GetParseContextFromMap(pXMLNode);
  CFX_CSSDisplay eDisplay = CFX_CSSDisplay::None;
  bool bContentNode = false;
  float fSpaceBelow = 0;
  RetainPtr<CFX_CSSComputedStyle> pStyle;
  WideString wsName;
  if (bEndBreak) {
    bool bCurOl = false;
    bool bCurLi = false;
    const CFX_XMLElement* pElement = nullptr;
    if (pContext) {
      if (m_bBlockContinue || (m_pLoader && pXMLNode == m_pLoader->pXMLNode)) {
        m_bBlockContinue = true;
      }
      if (pXMLNode->GetType() == CFX_XMLNode::Type::kText) {
        bContentNode = true;
      } else if (pXMLNode->GetType() == CFX_XMLNode::Type::kElement) {
        pElement = static_cast<const CFX_XMLElement*>(pXMLNode);
        wsName = pElement->GetLocalTagName();
      }
      if (wsName.EqualsASCII("ol")) {
        bIsOl = true;
        bCurOl = true;
      }
      if (m_bBlockContinue || bContentNode == false) {
        eDisplay = pContext->GetDisplay();
        if (eDisplay != CFX_CSSDisplay::Block &&
            eDisplay != CFX_CSSDisplay::Inline &&
            eDisplay != CFX_CSSDisplay::ListItem) {
          return true;
        }

        pStyle = m_textParser.ComputeStyle(pXMLNode, pParentStyle.Get());
        InitBreak(bContentNode ? pParentStyle.Get() : pStyle.Get(), eDisplay,
                  textWidth, pXMLNode, pParentStyle.Get());
        if ((eDisplay == CFX_CSSDisplay::Block ||
             eDisplay == CFX_CSSDisplay::ListItem) &&
            pStyle &&
            (wsName.IsEmpty() ||
             !(wsName.EqualsASCII("body") || wsName.EqualsASCII("html") ||
               wsName.EqualsASCII("ol") || wsName.EqualsASCII("ul")))) {
          const CFX_CSSRect* pRect = pStyle->GetMarginWidth();
          if (pRect) {
            *pLinePos += pRect->top.GetValue();
            fSpaceBelow = pRect->bottom.GetValue();
          }
        }

        if (wsName.EqualsASCII("a")) {
          ASSERT(pElement);
          WideString wsLinkContent = pElement->GetAttribute(L"href");
          if (!wsLinkContent.IsEmpty()) {
            pLinkData =
                pdfium::MakeRetain<CFX_LinkUserData>(wsLinkContent.c_str());
          }
        }

        int32_t iTabCount = m_textParser.CountTabs(
            bContentNode ? pParentStyle.Get() : pStyle.Get());
        bool bSpaceRun = m_textParser.IsSpaceRun(
            bContentNode ? pParentStyle.Get() : pStyle.Get());
        WideString wsText;
        if (bContentNode && iTabCount == 0) {
          wsText = ToXMLText(pXMLNode)->GetText();
        } else if (wsName.EqualsASCII("br")) {
          wsText = L'\n';
        } else if (wsName.EqualsASCII("li")) {
          bCurLi = true;
          if (bIsOl)
            wsText = WideString::Format(L"%d.  ", iLiCount);
          else
            wsText = 0x00B7 + WideStringView(L"  ", 1);
        } else if (!bContentNode) {
          if (iTabCount > 0) {
            while (iTabCount-- > 0)
              wsText += L'\t';
          } else {
            Optional<WideString> obj =
                m_textParser.GetEmbeddedObj(m_pTextProvider, pXMLNode);
            if (obj)
              wsText = *obj;
          }
        }

        int32_t iLength = wsText.GetLength();
        if (iLength > 0 && bContentNode && !bSpaceRun)
          ProcessText(&wsText);

        if (m_pLoader) {
          if (wsText.GetLength() > 0 && m_pLoader->bFilterSpace) {
            wsText.TrimLeft(L" ");
          }
          if (CFX_CSSDisplay::Block == eDisplay) {
            m_pLoader->bFilterSpace = true;
          } else if (CFX_CSSDisplay::Inline == eDisplay &&
                     m_pLoader->bFilterSpace) {
            m_pLoader->bFilterSpace = false;
          } else if (wsText.GetLength() > 0 &&
                     (0x20 == wsText[wsText.GetLength() - 1])) {
            m_pLoader->bFilterSpace = true;
          } else if (wsText.GetLength() != 0) {
            m_pLoader->bFilterSpace = false;
          }
        }

        if (wsText.GetLength() > 0) {
          if (!m_pLoader || m_pLoader->iChar == 0) {
            auto pUserData = pdfium::MakeRetain<CFX_TextUserData>(
                bContentNode ? pParentStyle : pStyle, pLinkData);
            m_pBreak->SetUserData(pUserData);
          }

          if (AppendChar(wsText, pLinePos, 0, bSavePieces)) {
            if (m_pLoader)
              m_pLoader->bFilterSpace = false;
            if (IsEnd(bSavePieces)) {
              if (m_pLoader && m_pLoader->iTotalLines > -1) {
                m_pLoader->pXMLNode = pXMLNode;
                m_pLoader->pParentStyle = pParentStyle;
              }
              return false;
            }
            return true;
          }
        }
      }
    }

    for (CFX_XMLNode* pChildNode = pXMLNode->GetFirstChild(); pChildNode;
         pChildNode = pChildNode->GetNextSibling()) {
      if (bCurOl)
        iLiCount++;

      if (!LoadRichText(pChildNode, textWidth, pLinePos,
                        pContext ? pStyle : pParentStyle, bSavePieces,
                        pLinkData, true, bIsOl, iLiCount))
        return false;
    }

    if (m_pLoader) {
      if (CFX_CSSDisplay::Block == eDisplay)
        m_pLoader->bFilterSpace = true;
    }
    if (bCurLi)
      EndBreak(CFX_BreakType::Line, pLinePos, bSavePieces);
  } else {
    if (pContext)
      eDisplay = pContext->GetDisplay();
  }

  if (m_bBlockContinue) {
    if (pContext && !bContentNode) {
      CFX_BreakType dwStatus = (eDisplay == CFX_CSSDisplay::Block)
                                   ? CFX_BreakType::Paragraph
                                   : CFX_BreakType::Piece;
      EndBreak(dwStatus, pLinePos, bSavePieces);
      if (eDisplay == CFX_CSSDisplay::Block) {
        *pLinePos += fSpaceBelow;
        if (m_pTabstopContext)
          m_pTabstopContext->RemoveAll();
      }
      if (IsEnd(bSavePieces)) {
        if (m_pLoader && m_pLoader->iTotalLines > -1) {
          m_pLoader->pXMLNode = pXMLNode->GetNextSibling();
          m_pLoader->pParentStyle = pParentStyle;
        }
        return false;
      }
    }
  }
  return true;
}

bool CXFA_TextLayout::AppendChar(const WideString& wsText,
                                 float* pLinePos,
                                 float fSpaceAbove,
                                 bool bSavePieces) {
  CFX_BreakType dwStatus = CFX_BreakType::None;
  int32_t iChar = 0;
  if (m_pLoader)
    iChar = m_pLoader->iChar;

  int32_t iLength = wsText.GetLength();
  for (int32_t i = iChar; i < iLength; i++) {
    wchar_t wch = wsText[i];
    if (wch == 0xA0)
      wch = 0x20;

    dwStatus = m_pBreak->AppendChar(wch);
    if (dwStatus != CFX_BreakType::None && dwStatus != CFX_BreakType::Piece) {
      AppendTextLine(dwStatus, pLinePos, bSavePieces, false);
      if (IsEnd(bSavePieces)) {
        if (m_pLoader)
          m_pLoader->iChar = i;
        return true;
      }
      if (dwStatus == CFX_BreakType::Paragraph && m_bRichText)
        *pLinePos += fSpaceAbove;
    }
  }
  if (m_pLoader)
    m_pLoader->iChar = 0;

  return false;
}

bool CXFA_TextLayout::IsEnd(bool bSavePieces) {
  if (!bSavePieces)
    return false;
  if (m_pLoader && m_pLoader->iTotalLines > 0)
    return m_iLines >= m_pLoader->iTotalLines;
  return false;
}

void CXFA_TextLayout::EndBreak(CFX_BreakType dwStatus,
                               float* pLinePos,
                               bool bSavePieces) {
  dwStatus = m_pBreak->EndBreak(dwStatus);
  if (dwStatus != CFX_BreakType::None && dwStatus != CFX_BreakType::Piece)
    AppendTextLine(dwStatus, pLinePos, bSavePieces, true);
}

void CXFA_TextLayout::DoTabstops(CFX_CSSComputedStyle* pStyle,
                                 CXFA_PieceLine* pPieceLine) {
  if (!pStyle || !pPieceLine)
    return;

  if (!m_pTabstopContext || m_pTabstopContext->m_tabstops.empty())
    return;

  int32_t iPieces = pdfium::CollectionSize<int32_t>(pPieceLine->m_textPieces);
  if (iPieces == 0)
    return;

  CXFA_TextPiece* pPiece = pPieceLine->m_textPieces[iPieces - 1].get();
  int32_t& iTabstopsIndex = m_pTabstopContext->m_iTabIndex;
  int32_t iCount = m_textParser.CountTabs(pStyle);
  if (!pdfium::IndexInBounds(m_pTabstopContext->m_tabstops, iTabstopsIndex))
    return;

  if (iCount > 0) {
    iTabstopsIndex++;
    m_pTabstopContext->m_bTabstops = true;
    float fRight = 0;
    if (iPieces > 1) {
      CXFA_TextPiece* p = pPieceLine->m_textPieces[iPieces - 2].get();
      fRight = p->rtPiece.right();
    }
    m_pTabstopContext->m_fTabWidth =
        pPiece->rtPiece.width + pPiece->rtPiece.left - fRight;
  } else if (iTabstopsIndex > -1) {
    float fLeft = 0;
    if (m_pTabstopContext->m_bTabstops) {
      uint32_t dwAlign = m_pTabstopContext->m_tabstops[iTabstopsIndex].dwAlign;
      if (dwAlign == FX_HashCode_GetW(L"center", false)) {
        fLeft = pPiece->rtPiece.width / 2.0f;
      } else if (dwAlign == FX_HashCode_GetW(L"right", false) ||
                 dwAlign == FX_HashCode_GetW(L"before", false)) {
        fLeft = pPiece->rtPiece.width;
      } else if (dwAlign == FX_HashCode_GetW(L"decimal", false)) {
        int32_t iChars = pPiece->iChars;
        for (int32_t i = 0; i < iChars; i++) {
          if (pPiece->szText[i] == L'.')
            break;

          fLeft += pPiece->Widths[i] / 20000.0f;
        }
      }
      m_pTabstopContext->m_fLeft =
          std::min(fLeft, m_pTabstopContext->m_fTabWidth);
      m_pTabstopContext->m_bTabstops = false;
      m_pTabstopContext->m_fTabWidth = 0;
    }
    pPiece->rtPiece.left -= m_pTabstopContext->m_fLeft;
  }
}

void CXFA_TextLayout::AppendTextLine(CFX_BreakType dwStatus,
                                     float* pLinePos,
                                     bool bSavePieces,
                                     bool bEndBreak) {
  int32_t iPieces = m_pBreak->CountBreakPieces();
  if (iPieces < 1)
    return;

  RetainPtr<CFX_CSSComputedStyle> pStyle;
  if (bSavePieces) {
    auto pNew = pdfium::MakeUnique<CXFA_PieceLine>();
    CXFA_PieceLine* pPieceLine = pNew.get();
    m_pieceLines.push_back(std::move(pNew));
    if (m_pTabstopContext)
      m_pTabstopContext->Reset();

    float fLineStep = 0, fBaseLine = 0;
    int32_t i = 0;
    for (i = 0; i < iPieces; i++) {
      const CFX_BreakPiece* pPiece = m_pBreak->GetBreakPieceUnstable(i);
      CFX_TextUserData* pUserData = pPiece->m_pUserData.Get();
      if (pUserData)
        pStyle = pUserData->m_pStyle;
      float fVerScale = pPiece->m_iVerticalScale / 100.0f;

      auto pTP = pdfium::MakeUnique<CXFA_TextPiece>();
      pTP->iChars = pPiece->m_iChars;
      pTP->szText = pPiece->GetString();
      pTP->Widths = pPiece->GetWidths();
      pTP->iBidiLevel = pPiece->m_iBidiLevel;
      pTP->iHorScale = pPiece->m_iHorizontalScale;
      pTP->iVerScale = pPiece->m_iVerticalScale;
      m_textParser.GetUnderline(m_pTextProvider, pStyle.Get(), pTP->iUnderline,
                                pTP->iPeriod);
      m_textParser.GetLinethrough(m_pTextProvider, pStyle.Get(),
                                  pTP->iLineThrough);
      pTP->dwColor = m_textParser.GetColor(m_pTextProvider, pStyle.Get());
      pTP->pFont =
          m_textParser.GetFont(m_pDoc.Get(), m_pTextProvider, pStyle.Get());
      pTP->fFontSize = m_textParser.GetFontSize(m_pTextProvider, pStyle.Get());
      pTP->rtPiece.left = pPiece->m_iStartPos / 20000.0f;
      pTP->rtPiece.width = pPiece->m_iWidth / 20000.0f;
      pTP->rtPiece.height =
          static_cast<float>(pPiece->m_iFontSize) * fVerScale / 20.0f;
      float fBaseLineTemp =
          m_textParser.GetBaseline(m_pTextProvider, pStyle.Get());
      pTP->rtPiece.top = fBaseLineTemp;

      float fLineHeight = m_textParser.GetLineHeight(
          m_pTextProvider, pStyle.Get(), m_iLines == 0, fVerScale);
      if (fBaseLineTemp > 0) {
        float fLineHeightTmp = fBaseLineTemp + pTP->rtPiece.height;
        if (fLineHeight < fLineHeightTmp)
          fLineHeight = fLineHeightTmp;
      }
      fLineStep = std::max(fLineStep, fLineHeight);
      pTP->pLinkData = pUserData ? pUserData->m_pLinkData : nullptr;
      pPieceLine->m_textPieces.push_back(std::move(pTP));
      DoTabstops(pStyle.Get(), pPieceLine);
    }
    for (const auto& pTP : pPieceLine->m_textPieces) {
      float& fTop = pTP->rtPiece.top;
      float fBaseLineTemp = fTop;
      fTop = *pLinePos + fLineStep - pTP->rtPiece.height - fBaseLineTemp;
      fTop = std::max(0.0f, fTop);
    }
    *pLinePos += fLineStep + fBaseLine;
  } else {
    float fLineStep = 0;
    float fLineWidth = 0;
    for (int32_t i = 0; i < iPieces; i++) {
      const CFX_BreakPiece* pPiece = m_pBreak->GetBreakPieceUnstable(i);
      CFX_TextUserData* pUserData = pPiece->m_pUserData.Get();
      if (pUserData)
        pStyle = pUserData->m_pStyle;
      float fVerScale = pPiece->m_iVerticalScale / 100.0f;
      float fBaseLine = m_textParser.GetBaseline(m_pTextProvider, pStyle.Get());
      float fLineHeight = m_textParser.GetLineHeight(
          m_pTextProvider, pStyle.Get(), m_iLines == 0, fVerScale);
      if (fBaseLine > 0) {
        float fLineHeightTmp =
            fBaseLine +
            static_cast<float>(pPiece->m_iFontSize) * fVerScale / 20.0f;
        if (fLineHeight < fLineHeightTmp) {
          fLineHeight = fLineHeightTmp;
        }
      }
      fLineStep = std::max(fLineStep, fLineHeight);
      fLineWidth += pPiece->m_iWidth / 20000.0f;
    }
    *pLinePos += fLineStep;
    m_fMaxWidth = std::max(m_fMaxWidth, fLineWidth);
    if (m_pLoader && m_pLoader->bSaveLineHeight) {
      float fHeight = *pLinePos - m_pLoader->fLastPos;
      m_pLoader->fLastPos = *pLinePos;
      m_pLoader->lineHeights.push_back(fHeight);
    }
  }

  m_pBreak->ClearBreakPieces();
  if (dwStatus == CFX_BreakType::Paragraph) {
    m_pBreak->Reset();
    if (!pStyle && bEndBreak) {
      CXFA_Para* para = m_pTextProvider->GetParaIfExists();
      if (para) {
        float fStartPos = para->GetMarginLeft();
        float fIndent = para->GetTextIndent();
        if (fIndent > 0)
          fStartPos += fIndent;

        float fSpaceBelow = para->GetSpaceBelow();
        if (fSpaceBelow < 0.1f)
          fSpaceBelow = 0;

        m_pBreak->SetLineStartPos(fStartPos);
        *pLinePos += fSpaceBelow;
      }
    }
  }

  if (pStyle) {
    float fStart = 0;
    const CFX_CSSRect* pRect = pStyle->GetMarginWidth();
    if (pRect)
      fStart = pRect->left.GetValue();

    float fTextIndent = pStyle->GetTextIndent().GetValue();
    if (fTextIndent < 0)
      fStart -= fTextIndent;

    m_pBreak->SetLineStartPos(fStart);
  }
  m_iLines++;
}

void CXFA_TextLayout::RenderString(CFX_RenderDevice* pDevice,
                                   CXFA_PieceLine* pPieceLine,
                                   size_t szPiece,
                                   std::vector<TextCharPos>* pCharPos,
                                   const CFX_Matrix& mtDoc2Device) {
  const CXFA_TextPiece* pPiece = pPieceLine->m_textPieces[szPiece].get();
  size_t szCount = GetDisplayPos(pPiece, pCharPos);
  if (szCount > 0) {
    auto span = pdfium::make_span(pCharPos->data(), szCount);
    CFDE_TextOut::DrawString(pDevice, pPiece->dwColor, pPiece->pFont, span,
                             pPiece->fFontSize, mtDoc2Device);
  }
  pPieceLine->m_charCounts.push_back(szCount);
}

void CXFA_TextLayout::RenderPath(CFX_RenderDevice* pDevice,
                                 CXFA_PieceLine* pPieceLine,
                                 size_t szPiece,
                                 std::vector<TextCharPos>* pCharPos,
                                 const CFX_Matrix& mtDoc2Device) {
  CXFA_TextPiece* pPiece = pPieceLine->m_textPieces[szPiece].get();
  bool bNoUnderline = pPiece->iUnderline < 1 || pPiece->iUnderline > 2;
  bool bNoLineThrough = pPiece->iLineThrough < 1 || pPiece->iLineThrough > 2;
  if (bNoUnderline && bNoLineThrough)
    return;

  CFX_PathData path;
  size_t szChars = GetDisplayPos(pPiece, pCharPos);
  if (szChars > 0) {
    CFX_PointF pt1;
    CFX_PointF pt2;
    float fEndY = (*pCharPos)[0].m_Origin.y + 1.05f;
    if (pPiece->iPeriod == XFA_AttributeValue::Word) {
      for (int32_t i = 0; i < pPiece->iUnderline; i++) {
        for (size_t j = 0; j < szChars; j++) {
          pt1.x = (*pCharPos)[j].m_Origin.x;
          pt2.x = pt1.x +
                  (*pCharPos)[j].m_FontCharWidth * pPiece->fFontSize / 1000.0f;
          pt1.y = pt2.y = fEndY;
          path.AppendLine(pt1, pt2);
        }
        fEndY += 2.0f;
      }
    } else {
      pt1.x = (*pCharPos)[0].m_Origin.x;
      pt2.x = (*pCharPos)[szChars - 1].m_Origin.x +
              (*pCharPos)[szChars - 1].m_FontCharWidth * pPiece->fFontSize /
                  1000.0f;
      for (int32_t i = 0; i < pPiece->iUnderline; i++) {
        pt1.y = pt2.y = fEndY;
        path.AppendLine(pt1, pt2);
        fEndY += 2.0f;
      }
    }
    fEndY = (*pCharPos)[0].m_Origin.y - pPiece->rtPiece.height * 0.25f;
    pt1.x = (*pCharPos)[0].m_Origin.x;
    pt2.x =
        (*pCharPos)[szChars - 1].m_Origin.x +
        (*pCharPos)[szChars - 1].m_FontCharWidth * pPiece->fFontSize / 1000.0f;
    for (int32_t i = 0; i < pPiece->iLineThrough; i++) {
      pt1.y = pt2.y = fEndY;
      path.AppendLine(pt1, pt2);
      fEndY += 2.0f;
    }
  } else {
    if (bNoLineThrough &&
        (bNoUnderline || pPiece->iPeriod != XFA_AttributeValue::All)) {
      return;
    }
    bool bHasCount = false;
    size_t szPiecePrev = szPiece;
    size_t szPieceNext = szPiece;
    while (szPiecePrev > 0) {
      szPiecePrev--;
      if (pPieceLine->m_charCounts[szPiecePrev] > 0) {
        bHasCount = true;
        break;
      }
    }
    if (!bHasCount)
      return;

    bHasCount = false;
    while (szPieceNext < pPieceLine->m_textPieces.size() - 1) {
      szPieceNext++;
      if (pPieceLine->m_charCounts[szPieceNext] > 0) {
        bHasCount = true;
        break;
      }
    }
    if (!bHasCount)
      return;

    float fOrgX = 0.0f;
    float fEndX = 0.0f;
    pPiece = pPieceLine->m_textPieces[szPiecePrev].get();
    szChars = GetDisplayPos(pPiece, pCharPos);
    if (szChars < 1)
      return;

    fOrgX =
        (*pCharPos)[szChars - 1].m_Origin.x +
        (*pCharPos)[szChars - 1].m_FontCharWidth * pPiece->fFontSize / 1000.0f;
    pPiece = pPieceLine->m_textPieces[szPieceNext].get();
    szChars = GetDisplayPos(pPiece, pCharPos);
    if (szChars < 1)
      return;

    fEndX = (*pCharPos)[0].m_Origin.x;
    CFX_PointF pt1;
    CFX_PointF pt2;
    pt1.x = fOrgX;
    pt2.x = fEndX;
    float fEndY = (*pCharPos)[0].m_Origin.y + 1.05f;
    for (int32_t i = 0; i < pPiece->iUnderline; i++) {
      pt1.y = fEndY;
      pt2.y = fEndY;
      path.AppendLine(pt1, pt2);
      fEndY += 2.0f;
    }
    fEndY = (*pCharPos)[0].m_Origin.y - pPiece->rtPiece.height * 0.25f;
    for (int32_t i = 0; i < pPiece->iLineThrough; i++) {
      pt1.y = fEndY;
      pt2.y = fEndY;
      path.AppendLine(pt1, pt2);
      fEndY += 2.0f;
    }
  }

  CFX_GraphStateData graphState;
  graphState.m_LineCap = CFX_GraphStateData::LineCapButt;
  graphState.m_LineJoin = CFX_GraphStateData::LineJoinMiter;
  graphState.m_LineWidth = 1;
  graphState.m_MiterLimit = 10;
  graphState.m_DashPhase = 0;
  pDevice->DrawPath(&path, &mtDoc2Device, &graphState, 0, pPiece->dwColor, 0);
}

size_t CXFA_TextLayout::GetDisplayPos(const CXFA_TextPiece* pPiece,
                                      std::vector<TextCharPos>* pCharPos) {
  if (!pPiece || pPiece->iChars < 1)
    return 0;
  return m_pBreak->GetDisplayPos(pPiece, pCharPos);
}
