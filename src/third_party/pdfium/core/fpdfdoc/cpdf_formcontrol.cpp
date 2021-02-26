// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_formcontrol.h"

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fpdfdoc/cpdf_interactiveform.h"
#include "third_party/base/stl_util.h"

namespace {

constexpr char kHighlightModes[] = {'N', 'I', 'O', 'P', 'T'};

// Order of |kHighlightModes| must match order of HighlightingMode enum.
static_assert(kHighlightModes[CPDF_FormControl::None] == 'N',
              "HighlightingMode mismatch");
static_assert(kHighlightModes[CPDF_FormControl::Invert] == 'I',
              "HighlightingMode mismatch");
static_assert(kHighlightModes[CPDF_FormControl::Outline] == 'O',
              "HighlightingMode mismatch");
static_assert(kHighlightModes[CPDF_FormControl::Push] == 'P',
              "HighlightingMode mismatch");
static_assert(kHighlightModes[CPDF_FormControl::Toggle] == 'T',
              "HighlightingMode mismatch");

}  // namespace

CPDF_FormControl::CPDF_FormControl(CPDF_FormField* pField,
                                   CPDF_Dictionary* pWidgetDict)
    : m_pField(pField),
      m_pWidgetDict(pWidgetDict),
      m_pForm(m_pField->GetForm()) {}

CPDF_FormControl::~CPDF_FormControl() = default;

CFX_FloatRect CPDF_FormControl::GetRect() const {
  return m_pWidgetDict->GetRectFor("Rect");
}

ByteString CPDF_FormControl::GetOnStateName() const {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  CPDF_Dictionary* pAP = m_pWidgetDict->GetDictFor("AP");
  if (!pAP)
    return ByteString();

  CPDF_Dictionary* pN = pAP->GetDictFor("N");
  if (!pN)
    return ByteString();

  CPDF_DictionaryLocker locker(pN);
  for (const auto& it : locker) {
    if (it.first != "Off")
      return it.first;
  }
  return ByteString();
}

ByteString CPDF_FormControl::GetCheckedAPState() const {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  ByteString csOn = GetOnStateName();
  if (ToArray(CPDF_FormField::GetFieldAttr(m_pField->GetDict(), "Opt")))
    csOn = ByteString::Format("%d", m_pField->GetControlIndex(this));
  if (csOn.IsEmpty())
    csOn = "Yes";
  return csOn;
}

WideString CPDF_FormControl::GetExportValue() const {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  ByteString csOn = GetOnStateName();
  CPDF_Array* pArray =
      ToArray(CPDF_FormField::GetFieldAttr(m_pField->GetDict(), "Opt"));
  if (pArray)
    csOn = pArray->GetStringAt(m_pField->GetControlIndex(this));
  if (csOn.IsEmpty())
    csOn = "Yes";
  return PDF_DecodeText(csOn.raw_span());
}

bool CPDF_FormControl::IsChecked() const {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  ByteString csOn = GetOnStateName();
  ByteString csAS = m_pWidgetDict->GetStringFor("AS");
  return csAS == csOn;
}

bool CPDF_FormControl::IsDefaultChecked() const {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  CPDF_Object* pDV = CPDF_FormField::GetFieldAttr(m_pField->GetDict(), "DV");
  if (!pDV)
    return false;

  ByteString csDV = pDV->GetString();
  ByteString csOn = GetOnStateName();
  return (csDV == csOn);
}

void CPDF_FormControl::CheckControl(bool bChecked) {
  ASSERT(GetType() == CPDF_FormField::kCheckBox ||
         GetType() == CPDF_FormField::kRadioButton);
  ByteString csOldAS = m_pWidgetDict->GetStringFor("AS", "Off");
  ByteString csAS = "Off";
  if (bChecked)
    csAS = GetOnStateName();
  if (csOldAS == csAS)
    return;
  m_pWidgetDict->SetNewFor<CPDF_Name>("AS", csAS);
}

CPDF_FormControl::HighlightingMode CPDF_FormControl::GetHighlightingMode()
    const {
  if (!m_pWidgetDict)
    return Invert;

  ByteString csH = m_pWidgetDict->GetStringFor("H", "I");
  for (size_t i = 0; i < pdfium::size(kHighlightModes); ++i) {
    if (csH == kHighlightModes[i])
      return static_cast<HighlightingMode>(i);
  }
  return Invert;
}

CPDF_ApSettings CPDF_FormControl::GetMK() const {
  return CPDF_ApSettings(m_pWidgetDict ? m_pWidgetDict->GetDictFor("MK")
                                       : nullptr);
}

bool CPDF_FormControl::HasMKEntry(const ByteString& csEntry) const {
  return GetMK().HasMKEntry(csEntry);
}

int CPDF_FormControl::GetRotation() const {
  return GetMK().GetRotation();
}

FX_ARGB CPDF_FormControl::GetColor(int& iColorType, const ByteString& csEntry) {
  return GetMK().GetColor(iColorType, csEntry);
}

float CPDF_FormControl::GetOriginalColor(int index, const ByteString& csEntry) {
  return GetMK().GetOriginalColor(index, csEntry);
}

void CPDF_FormControl::GetOriginalColor(int& iColorType,
                                        float fc[4],
                                        const ByteString& csEntry) {
  GetMK().GetOriginalColor(iColorType, fc, csEntry);
}

WideString CPDF_FormControl::GetCaption(const ByteString& csEntry) const {
  return GetMK().GetCaption(csEntry);
}

CPDF_Stream* CPDF_FormControl::GetIcon(const ByteString& csEntry) {
  return GetMK().GetIcon(csEntry);
}

CPDF_IconFit CPDF_FormControl::GetIconFit() const {
  return GetMK().GetIconFit();
}

int CPDF_FormControl::GetTextPosition() const {
  return GetMK().GetTextPosition();
}

CPDF_DefaultAppearance CPDF_FormControl::GetDefaultAppearance() const {
  if (!m_pWidgetDict)
    return CPDF_DefaultAppearance();

  if (m_pWidgetDict->KeyExist("DA"))
    return CPDF_DefaultAppearance(m_pWidgetDict->GetStringFor("DA"));

  CPDF_Object* pObj = CPDF_FormField::GetFieldAttr(m_pField->GetDict(), "DA");
  if (!pObj)
    return m_pForm->GetDefaultAppearance();
  return CPDF_DefaultAppearance(pObj->GetString());
}

Optional<WideString> CPDF_FormControl::GetDefaultControlFontName() const {
  RetainPtr<CPDF_Font> pFont = GetDefaultControlFont();
  if (!pFont)
    return {};

  return WideString::FromDefANSI(pFont->GetBaseFontName().AsStringView());
}

RetainPtr<CPDF_Font> CPDF_FormControl::GetDefaultControlFont() const {
  float fFontSize;
  CPDF_DefaultAppearance cDA = GetDefaultAppearance();
  Optional<ByteString> csFontNameTag = cDA.GetFont(&fFontSize);
  if (!csFontNameTag || csFontNameTag->IsEmpty())
    return nullptr;

  CPDF_Object* pObj = CPDF_FormField::GetFieldAttr(m_pWidgetDict.Get(), "DR");
  if (CPDF_Dictionary* pDict = ToDictionary(pObj)) {
    CPDF_Dictionary* pFonts = pDict->GetDictFor("Font");
    if (ValidateFontResourceDict(pFonts)) {
      CPDF_Dictionary* pElement = pFonts->GetDictFor(*csFontNameTag);
      if (pElement) {
        auto* pData = CPDF_DocPageData::FromDocument(m_pForm->GetDocument());
        RetainPtr<CPDF_Font> pFont = pData->GetFont(pElement);
        if (pFont)
          return pFont;
      }
    }
  }
  RetainPtr<CPDF_Font> pFormFont = m_pForm->GetFormFont(*csFontNameTag);
  if (pFormFont)
    return pFormFont;

  CPDF_Dictionary* pPageDict = m_pWidgetDict->GetDictFor("P");
  CPDF_Dictionary* pDict =
      ToDictionary(CPDF_FormField::GetFieldAttr(pPageDict, "Resources"));
  if (!pDict)
    return nullptr;

  CPDF_Dictionary* pFonts = pDict->GetDictFor("Font");
  if (!ValidateFontResourceDict(pFonts))
    return nullptr;

  CPDF_Dictionary* pElement = pFonts->GetDictFor(*csFontNameTag);
  if (!pElement)
    return nullptr;

  auto* pDocPageData = CPDF_DocPageData::FromDocument(m_pForm->GetDocument());
  return pDocPageData->GetFont(pElement);
}

int CPDF_FormControl::GetControlAlignment() const {
  if (!m_pWidgetDict)
    return 0;
  if (m_pWidgetDict->KeyExist("Q"))
    return m_pWidgetDict->GetIntegerFor("Q", 0);

  CPDF_Object* pObj = CPDF_FormField::GetFieldAttr(m_pField->GetDict(), "Q");
  if (pObj)
    return pObj->GetInteger();
  return m_pForm->GetFormAlignment();
}
