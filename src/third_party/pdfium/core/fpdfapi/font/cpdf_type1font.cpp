// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/font/cpdf_type1font.h"

#include <algorithm>
#include <iterator>

#include "build/build_config.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/span_util.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/freetype/fx_freetype.h"
#include "core/fxge/fx_font.h"

#if BUILDFLAG(IS_APPLE)
#include <Carbon/Carbon.h>
#endif  // BUILDFLAG(IS_APPLE)

namespace {

#if BUILDFLAG(IS_APPLE)
struct GlyphNameMap {
  const char* m_pStrAdobe;    // Raw, POD struct.
  const char* m_pStrUnicode;  // Raw, POD struct.
};

const GlyphNameMap kGlyphNameSubsts[] = {{"ff", "uniFB00"},
                                         {"ffi", "uniFB03"},
                                         {"ffl", "uniFB04"},
                                         {"fi", "uniFB01"},
                                         {"fl", "uniFB02"}};

const char* GlyphNameRemap(const char* pStrAdobe) {
  for (const auto& element : kGlyphNameSubsts) {
    if (!FXSYS_stricmp(element.m_pStrAdobe, pStrAdobe))
      return element.m_pStrUnicode;
  }
  return nullptr;
}

#endif  // BUILDFLAG(IS_APPLE)

bool FT_UseType1Charmap(FXFT_FaceRec* face) {
  if (face->num_charmaps == 0)
    return false;

  bool is_first_charmap_unicode =
      FXFT_Get_Charmap_Encoding(face->charmaps[0]) == FT_ENCODING_UNICODE;
  if (face->num_charmaps == 1 && is_first_charmap_unicode)
    return false;

  int index = is_first_charmap_unicode ? 1 : 0;
  FT_Set_Charmap(face, face->charmaps[index]);
  return true;
}

}  // namespace

CPDF_Type1Font::CPDF_Type1Font(CPDF_Document* pDocument,
                               CPDF_Dictionary* pFontDict)
    : CPDF_SimpleFont(pDocument, pFontDict) {
#if BUILDFLAG(IS_APPLE)
  memset(m_ExtGID, 0xff, sizeof(m_ExtGID));
#endif
}

CPDF_Type1Font::~CPDF_Type1Font() = default;

bool CPDF_Type1Font::IsType1Font() const {
  return true;
}

const CPDF_Type1Font* CPDF_Type1Font::AsType1Font() const {
  return this;
}

CPDF_Type1Font* CPDF_Type1Font::AsType1Font() {
  return this;
}

bool CPDF_Type1Font::Load() {
  m_Base14Font = CFX_FontMapper::GetStandardFontName(&m_BaseFontName);
  if (!IsBase14Font())
    return LoadCommon();

  const CPDF_Dictionary* pFontDesc = m_pFontDict->GetDictFor("FontDescriptor");
  if (pFontDesc && pFontDesc->KeyExist("Flags")) {
    m_Flags = pFontDesc->GetIntegerFor("Flags");
  } else if (IsSymbolicFont()) {
    m_Flags = FXFONT_SYMBOLIC;
  } else {
    m_Flags = FXFONT_NONSYMBOLIC;
  }
  if (IsFixedFont()) {
    std::fill(std::begin(m_CharWidth), std::end(m_CharWidth), 600);
  }
  if (m_Base14Font == CFX_FontMapper::kSymbol)
    m_BaseEncoding = FontEncoding::kAdobeSymbol;
  else if (m_Base14Font == CFX_FontMapper::kDingbats)
    m_BaseEncoding = FontEncoding::kZapfDingbats;
  else if (FontStyleIsNonSymbolic(m_Flags))
    m_BaseEncoding = FontEncoding::kStandard;
  return LoadCommon();
}

#if BUILDFLAG(IS_APPLE)
int CPDF_Type1Font::GlyphFromCharCodeExt(uint32_t charcode) {
  if (charcode > 0xff)
    return -1;

  int index = m_ExtGID[static_cast<uint8_t>(charcode)];
  return index != 0xffff ? index : -1;
}
#endif

void CPDF_Type1Font::LoadGlyphMap() {
  if (!m_Font.GetFaceRec())
    return;

#if BUILDFLAG(IS_APPLE)
  bool bCoreText = true;
  if (!m_Font.GetPlatformFont()) {
    if (m_Font.GetPsName() == "DFHeiStd-W5")
      bCoreText = false;

    auto* pPlatform = CFX_GEModule::Get()->GetPlatform();
    pdfium::span<const uint8_t> span = m_Font.GetFontSpan();
    m_Font.SetPlatformFont(pPlatform->CreatePlatformFont(span));
    if (!m_Font.GetPlatformFont())
      bCoreText = false;
  }
#endif
  if (!IsEmbedded() && !IsSymbolicFont() && m_Font.IsTTFont()) {
    if (UseTTCharmapMSSymbol(m_Font.GetFaceRec())) {
      bool bGotOne = false;
      for (uint32_t charcode = 0; charcode < kInternalTableSize; charcode++) {
        const uint8_t prefix[4] = {0x00, 0xf0, 0xf1, 0xf2};
        for (int j = 0; j < 4; j++) {
          uint16_t unicode = prefix[j] * 256 + charcode;
          m_GlyphIndex[charcode] =
              FT_Get_Char_Index(m_Font.GetFaceRec(), unicode);
#if BUILDFLAG(IS_APPLE)
          CalcExtGID(charcode);
#endif
          if (m_GlyphIndex[charcode]) {
            bGotOne = true;
            break;
          }
        }
      }
      if (bGotOne) {
#if BUILDFLAG(IS_APPLE)
        if (!bCoreText)
          memcpy(m_ExtGID, m_GlyphIndex, sizeof(m_ExtGID));
#endif
        return;
      }
    }
    FXFT_Select_Charmap(m_Font.GetFaceRec(), FT_ENCODING_UNICODE);
    if (m_BaseEncoding == FontEncoding::kBuiltin)
      m_BaseEncoding = FontEncoding::kStandard;

    for (uint32_t charcode = 0; charcode < kInternalTableSize; charcode++) {
      const char* name =
          GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
      if (!name)
        continue;

      m_Encoding.SetUnicode(charcode, UnicodeFromAdobeName(name));
      m_GlyphIndex[charcode] = FT_Get_Char_Index(
          m_Font.GetFaceRec(), m_Encoding.UnicodeFromCharCode(charcode));
#if BUILDFLAG(IS_APPLE)
      CalcExtGID(charcode);
#endif
      if (m_GlyphIndex[charcode] == 0 && strcmp(name, ".notdef") == 0) {
        m_Encoding.SetUnicode(charcode, 0x20);
        m_GlyphIndex[charcode] = FT_Get_Char_Index(m_Font.GetFaceRec(), 0x20);
#if BUILDFLAG(IS_APPLE)
        CalcExtGID(charcode);
#endif
      }
    }
#if BUILDFLAG(IS_APPLE)
    if (!bCoreText) {
      fxcrt::spancpy(pdfium::make_span(m_ExtGID),
                     pdfium::make_span(m_GlyphIndex));
    }
#endif
    return;
  }
  FT_UseType1Charmap(m_Font.GetFaceRec());
#if BUILDFLAG(IS_APPLE)
  if (bCoreText) {
    if (FontStyleIsSymbolic(m_Flags)) {
      for (uint32_t charcode = 0; charcode < kInternalTableSize; charcode++) {
        const char* name =
            GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
        if (name) {
          m_Encoding.SetUnicode(charcode, UnicodeFromAdobeName(name));
          m_GlyphIndex[charcode] = FT_Get_Name_Index(m_Font.GetFaceRec(), name);
          SetExtGID(name, charcode);
        } else {
          m_GlyphIndex[charcode] =
              FT_Get_Char_Index(m_Font.GetFaceRec(), charcode);
          char name_glyph[kInternalTableSize] = {};
          FT_Get_Glyph_Name(m_Font.GetFaceRec(), m_GlyphIndex[charcode],
                            name_glyph, sizeof(name_glyph));
          name_glyph[kInternalTableSize - 1] = 0;
          const wchar_t unicode =
              name_glyph[0] != 0 ? UnicodeFromAdobeName(name_glyph) : 0;
          m_Encoding.SetUnicode(charcode, unicode);
          SetExtGID(name_glyph, charcode);
        }
      }
      return;
    }

    bool bUnicode =
        FXFT_Select_Charmap(m_Font.GetFaceRec(), FT_ENCODING_UNICODE) == 0;
    for (uint32_t charcode = 0; charcode < kInternalTableSize; charcode++) {
      const char* name =
          GetAdobeCharName(m_BaseEncoding, m_CharNames, charcode);
      if (!name)
        continue;

      m_Encoding.SetUnicode(charcode, UnicodeFromAdobeName(name));
      const char* pStrUnicode = GlyphNameRemap(name);
      if (pStrUnicode && FT_Get_Name_Index(m_Font.GetFaceRec(), name) == 0) {
        name = pStrUnicode;
      }
      m_GlyphIndex[charcode] = FT_Get_Name_Index(m_Font.GetFaceRec(), name);
      SetExtGID(name, charcode);
      if (m_GlyphIndex[charcode] != 0)
        continue;

      if (strcmp(name, ".notdef") != 0 && strcmp(name, "space") != 0) {
        m_GlyphIndex[charcode] = FT_Get_Char_Index(
            m_Font.GetFaceRec(),
            bUnicode ? m_Encoding.UnicodeFromCharCode(charcode) : charcode);
        CalcExtGID(charcode);
      } else {
        m_Encoding.SetUnicode(charcode, 0x20);
        m_GlyphIndex[charcode] =
            bUnicode ? FT_Get_Char_Index(m_Font.GetFaceRec(), 0x20) : 0xffff;
        CalcExtGID(charcode);
      }
    }
    return;
  }
#endif  // BUILDFLAG(IS_APPLE)
  if (FontStyleIsSymbolic(m_Flags)) {
    for (size_t charcode = 0; charcode < kInternalTableSize; charcode++) {
      const char* name = GetAdobeCharName(m_BaseEncoding, m_CharNames,
                                          static_cast<uint32_t>(charcode));
      if (name) {
        m_Encoding.SetUnicode(charcode, UnicodeFromAdobeName(name));
        m_GlyphIndex[charcode] = FT_Get_Name_Index(m_Font.GetFaceRec(), name);
      } else {
        m_GlyphIndex[charcode] = FT_Get_Char_Index(
            m_Font.GetFaceRec(), static_cast<uint32_t>(charcode));
        if (m_GlyphIndex[charcode]) {
          char name_glyph[kInternalTableSize] = {};
          FT_Get_Glyph_Name(m_Font.GetFaceRec(), m_GlyphIndex[charcode],
                            name_glyph, sizeof(name_glyph));
          name_glyph[kInternalTableSize - 1] = 0;
          const wchar_t unicode =
              name_glyph[0] != 0 ? UnicodeFromAdobeName(name_glyph) : 0;
          m_Encoding.SetUnicode(charcode, unicode);
        }
      }
    }
#if BUILDFLAG(IS_APPLE)
    if (!bCoreText)
      memcpy(m_ExtGID, m_GlyphIndex, sizeof(m_ExtGID));
#endif
    return;
  }

  bool bUnicode =
      FXFT_Select_Charmap(m_Font.GetFaceRec(), FT_ENCODING_UNICODE) == 0;
  for (size_t charcode = 0; charcode < kInternalTableSize; charcode++) {
    const char* name = GetAdobeCharName(m_BaseEncoding, m_CharNames,
                                        static_cast<uint32_t>(charcode));
    if (!name)
      continue;

    m_Encoding.SetUnicode(charcode, UnicodeFromAdobeName(name));
    m_GlyphIndex[charcode] = FT_Get_Name_Index(m_Font.GetFaceRec(), name);
    if (m_GlyphIndex[charcode] != 0)
      continue;

    if (strcmp(name, ".notdef") != 0 && strcmp(name, "space") != 0) {
      m_GlyphIndex[charcode] =
          FT_Get_Char_Index(m_Font.GetFaceRec(),
                            bUnicode ? m_Encoding.UnicodeFromCharCode(charcode)
                                     : static_cast<uint32_t>(charcode));
    } else {
      m_Encoding.SetUnicode(charcode, 0x20);
      m_GlyphIndex[charcode] = 0xffff;
    }
  }
#if BUILDFLAG(IS_APPLE)
  if (!bCoreText)
    memcpy(m_ExtGID, m_GlyphIndex, sizeof(m_ExtGID));
#endif
}

bool CPDF_Type1Font::IsSymbolicFont() const {
  return m_Base14Font.has_value() &&
         CFX_FontMapper::IsSymbolicFont(m_Base14Font.value());
}

bool CPDF_Type1Font::IsFixedFont() const {
  return m_Base14Font.has_value() &&
         CFX_FontMapper::IsFixedFont(m_Base14Font.value());
}

#if BUILDFLAG(IS_APPLE)
void CPDF_Type1Font::SetExtGID(const char* name, uint32_t charcode) {
  CFStringRef name_ct = CFStringCreateWithCStringNoCopy(
      kCFAllocatorDefault, name, kCFStringEncodingASCII, kCFAllocatorNull);
  m_ExtGID[charcode] =
      CGFontGetGlyphWithGlyphName((CGFontRef)m_Font.GetPlatformFont(), name_ct);
  if (name_ct)
    CFRelease(name_ct);
}

void CPDF_Type1Font::CalcExtGID(uint32_t charcode) {
  char name_glyph[kInternalTableSize] = {};
  FT_Get_Glyph_Name(m_Font.GetFaceRec(), m_GlyphIndex[charcode], name_glyph,
                    sizeof(name_glyph));
  name_glyph[kInternalTableSize - 1] = 0;
  SetExtGID(name_glyph, charcode);
}
#endif  // BUILDFLAG(IS_APPLE)
