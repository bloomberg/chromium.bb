// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/android/cfpf_skiafontmgr.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "core/fxcrt/fx_codepage.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_folder.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/android/cfpf_skiafont.h"
#include "core/fxge/android/cfpf_skiapathfont.h"
#include "core/fxge/freetype/fx_freetype.h"
#include "core/fxge/fx_font.h"
#include "third_party/base/containers/adapters.h"

namespace {

constexpr int FPF_SKIAMATCHWEIGHT_NAME1 = 62;
constexpr int FPF_SKIAMATCHWEIGHT_NAME2 = 60;
constexpr int FPF_SKIAMATCHWEIGHT_1 = 16;
constexpr int FPF_SKIAMATCHWEIGHT_2 = 8;

struct FPF_SKIAFONTMAP {
  uint32_t dwFamily;
  uint32_t dwSubSt;
};

const FPF_SKIAFONTMAP kSkiaFontmap[] = {
    {0x58c5083, 0xc8d2e345},  {0x5dfade2, 0xe1633081},
    {0x684317d, 0xe1633081},  {0x14ee2d13, 0xc8d2e345},
    {0x3918fe2d, 0xbbeeec72}, {0x3b98b31c, 0xe1633081},
    {0x3d49f40e, 0xe1633081}, {0x432c41c5, 0xe1633081},
    {0x491b6ad0, 0xe1633081}, {0x5612cab1, 0x59b9f8f1},
    {0x779ce19d, 0xc8d2e345}, {0x7cc9510b, 0x59b9f8f1},
    {0x83746053, 0xbbeeec72}, {0xaaa60c03, 0xbbeeec72},
    {0xbf85ff26, 0xe1633081}, {0xc04fe601, 0xbbeeec72},
    {0xca3812d5, 0x59b9f8f1}, {0xca383e15, 0x59b9f8f1},
    {0xcad5eaf6, 0x59b9f8f1}, {0xcb7a04c8, 0xc8d2e345},
    {0xfb4ce0de, 0xe1633081},
};

const FPF_SKIAFONTMAP kSkiaSansFontMap[] = {
    {0x58c5083, 0xd5b8d10f},  {0x14ee2d13, 0xd5b8d10f},
    {0x779ce19d, 0xd5b8d10f}, {0xcb7a04c8, 0xd5b8d10f},
    {0xfb4ce0de, 0xd5b8d10f},
};

uint32_t FPF_SkiaGetSubstFont(uint32_t dwHash,
                              const FPF_SKIAFONTMAP* skFontMap,
                              size_t length) {
  const FPF_SKIAFONTMAP* pEnd = skFontMap + length;
  const FPF_SKIAFONTMAP* pFontMap = std::lower_bound(
      skFontMap, pEnd, dwHash, [](const FPF_SKIAFONTMAP& item, uint32_t hash) {
        return item.dwFamily < hash;
      });
  if (pFontMap < pEnd && pFontMap->dwFamily == dwHash)
    return pFontMap->dwSubSt;
  return 0;
}

enum FPF_SKIACHARSET {
  FPF_SKIACHARSET_Ansi = 1 << 0,
  FPF_SKIACHARSET_Default = 1 << 1,
  FPF_SKIACHARSET_Symbol = 1 << 2,
  FPF_SKIACHARSET_ShiftJIS = 1 << 3,
  FPF_SKIACHARSET_Korean = 1 << 4,
  FPF_SKIACHARSET_Johab = 1 << 5,
  FPF_SKIACHARSET_GB2312 = 1 << 6,
  FPF_SKIACHARSET_BIG5 = 1 << 7,
  FPF_SKIACHARSET_Greek = 1 << 8,
  FPF_SKIACHARSET_Turkish = 1 << 9,
  FPF_SKIACHARSET_Vietnamese = 1 << 10,
  FPF_SKIACHARSET_Hebrew = 1 << 11,
  FPF_SKIACHARSET_Arabic = 1 << 12,
  FPF_SKIACHARSET_Baltic = 1 << 13,
  FPF_SKIACHARSET_Cyrillic = 1 << 14,
  FPF_SKIACHARSET_Thai = 1 << 15,
  FPF_SKIACHARSET_EeasternEuropean = 1 << 16,
  FPF_SKIACHARSET_PC = 1 << 17,
  FPF_SKIACHARSET_OEM = 1 << 18,
};

uint32_t FPF_SkiaGetCharset(FX_Charset uCharset) {
  switch (uCharset) {
    case FX_Charset::kANSI:
      return FPF_SKIACHARSET_Ansi;
    case FX_Charset::kDefault:
      return FPF_SKIACHARSET_Default;
    case FX_Charset::kSymbol:
      return FPF_SKIACHARSET_Symbol;
    case FX_Charset::kShiftJIS:
      return FPF_SKIACHARSET_ShiftJIS;
    case FX_Charset::kHangul:
      return FPF_SKIACHARSET_Korean;
    case FX_Charset::kChineseSimplified:
      return FPF_SKIACHARSET_GB2312;
    case FX_Charset::kChineseTraditional:
      return FPF_SKIACHARSET_BIG5;
    case FX_Charset::kMSWin_Greek:
      return FPF_SKIACHARSET_Greek;
    case FX_Charset::kMSWin_Turkish:
      return FPF_SKIACHARSET_Turkish;
    case FX_Charset::kMSWin_Hebrew:
      return FPF_SKIACHARSET_Hebrew;
    case FX_Charset::kMSWin_Arabic:
      return FPF_SKIACHARSET_Arabic;
    case FX_Charset::kMSWin_Baltic:
      return FPF_SKIACHARSET_Baltic;
    case FX_Charset::kMSWin_Cyrillic:
      return FPF_SKIACHARSET_Cyrillic;
    case FX_Charset::kThai:
      return FPF_SKIACHARSET_Thai;
    case FX_Charset::kMSWin_EasternEuropean:
      return FPF_SKIACHARSET_EeasternEuropean;
  }
  return FPF_SKIACHARSET_Default;
}

uint32_t FPF_SKIANormalizeFontName(ByteStringView bsFamily) {
  uint32_t uHashCode = 0;
  for (unsigned char ch : bsFamily) {
    if (ch == ' ' || ch == '-' || ch == ',')
      continue;
    uHashCode = 31 * uHashCode + tolower(ch);
  }
  return uHashCode;
}

uint32_t FPF_SKIAGetFamilyHash(ByteStringView bsFamily,
                               uint32_t dwStyle,
                               FX_Charset uCharset) {
  ByteString bsFont(bsFamily);
  if (FontStyleIsForceBold(dwStyle))
    bsFont += "Bold";
  if (FontStyleIsItalic(dwStyle))
    bsFont += "Italic";
  if (FontStyleIsSerif(dwStyle))
    bsFont += "Serif";
  bsFont += static_cast<uint8_t>(uCharset);
  return FX_HashCode_GetA(bsFont.AsStringView());
}

bool FPF_SkiaIsCJK(FX_Charset uCharset) {
  return FX_CharSetIsCJK(uCharset);
}

bool FPF_SkiaMaybeSymbol(ByteStringView bsFacename) {
  ByteString bsName(bsFacename);
  bsName.MakeLower();
  return bsName.Contains("symbol");
}

bool FPF_SkiaMaybeArabic(ByteStringView bsFacename) {
  ByteString bsName(bsFacename);
  bsName.MakeLower();
  return bsName.Contains("arabic");
}

const uint32_t kFPFSkiaFontCharsets[] = {
    FPF_SKIACHARSET_Ansi,
    FPF_SKIACHARSET_EeasternEuropean,
    FPF_SKIACHARSET_Cyrillic,
    FPF_SKIACHARSET_Greek,
    FPF_SKIACHARSET_Turkish,
    FPF_SKIACHARSET_Hebrew,
    FPF_SKIACHARSET_Arabic,
    FPF_SKIACHARSET_Baltic,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    FPF_SKIACHARSET_Thai,
    FPF_SKIACHARSET_ShiftJIS,
    FPF_SKIACHARSET_GB2312,
    FPF_SKIACHARSET_Korean,
    FPF_SKIACHARSET_BIG5,
    FPF_SKIACHARSET_Johab,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    FPF_SKIACHARSET_OEM,
    FPF_SKIACHARSET_Symbol,
};

uint32_t FPF_SkiaGetFaceCharset(TT_OS2* pOS2) {
  uint32_t dwCharset = 0;
  if (pOS2) {
    for (int32_t i = 0; i < 32; i++) {
      if (pOS2->ulCodePageRange1 & (1 << i))
        dwCharset |= kFPFSkiaFontCharsets[i];
    }
  }
  dwCharset |= FPF_SKIACHARSET_Default;
  return dwCharset;
}

}  // namespace

CFPF_SkiaFontMgr::CFPF_SkiaFontMgr() = default;

CFPF_SkiaFontMgr::~CFPF_SkiaFontMgr() {
  m_FamilyFonts.clear();
  m_FontFaces.clear();
}

bool CFPF_SkiaFontMgr::InitFTLibrary() {
  if (m_FTLibrary)
    return true;

  FXFT_LibraryRec* pLibrary = nullptr;
  FT_Init_FreeType(&pLibrary);
  if (!pLibrary)
    return false;

  m_FTLibrary.reset(pLibrary);
  return true;
}

void CFPF_SkiaFontMgr::LoadSystemFonts() {
  if (m_bLoaded)
    return;
  ScanPath("/system/fonts");
  m_bLoaded = true;
}

CFPF_SkiaFont* CFPF_SkiaFontMgr::CreateFont(ByteStringView bsFamilyname,
                                            FX_Charset uCharset,
                                            uint32_t dwStyle) {
  uint32_t dwHash = FPF_SKIAGetFamilyHash(bsFamilyname, dwStyle, uCharset);
  auto family_iter = m_FamilyFonts.find(dwHash);
  if (family_iter != m_FamilyFonts.end())
    return family_iter->second.get();

  uint32_t dwFaceName = FPF_SKIANormalizeFontName(bsFamilyname);
  uint32_t dwSubst =
      FPF_SkiaGetSubstFont(dwFaceName, kSkiaFontmap, std::size(kSkiaFontmap));
  uint32_t dwSubstSans = FPF_SkiaGetSubstFont(dwFaceName, kSkiaSansFontMap,
                                              std::size(kSkiaSansFontMap));
  bool bMaybeSymbol = FPF_SkiaMaybeSymbol(bsFamilyname);
  if (uCharset != FX_Charset::kMSWin_Arabic &&
      FPF_SkiaMaybeArabic(bsFamilyname)) {
    uCharset = FX_Charset::kMSWin_Arabic;
  } else if (uCharset == FX_Charset::kANSI) {
    uCharset = FX_Charset::kDefault;
  }
  int32_t nExpectVal = FPF_SKIAMATCHWEIGHT_NAME1 + FPF_SKIAMATCHWEIGHT_1 * 3 +
                       FPF_SKIAMATCHWEIGHT_2 * 2;
  const CFPF_SkiaPathFont* pBestFont = nullptr;
  int32_t nMax = -1;
  int32_t nGlyphNum = 0;
  for (const std::unique_ptr<CFPF_SkiaPathFont>& font :
       pdfium::base::Reversed(m_FontFaces)) {
    if (!(font->charsets() & FPF_SkiaGetCharset(uCharset)))
      continue;
    int32_t nFind = 0;
    uint32_t dwSysFontName = FPF_SKIANormalizeFontName(font->family());
    if (dwFaceName == dwSysFontName)
      nFind += FPF_SKIAMATCHWEIGHT_NAME1;
    bool bMatchedName = (nFind == FPF_SKIAMATCHWEIGHT_NAME1);
    if (FontStyleIsForceBold(dwStyle) == FontStyleIsForceBold(font->style()))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if (FontStyleIsItalic(dwStyle) == FontStyleIsItalic(font->style()))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if (FontStyleIsFixedPitch(dwStyle) ==
        FontStyleIsFixedPitch(font->style())) {
      nFind += FPF_SKIAMATCHWEIGHT_2;
    }
    if (FontStyleIsSerif(dwStyle) == FontStyleIsSerif(font->style()))
      nFind += FPF_SKIAMATCHWEIGHT_1;
    if (FontStyleIsScript(dwStyle) == FontStyleIsScript(font->style()))
      nFind += FPF_SKIAMATCHWEIGHT_2;
    if (dwSubst == dwSysFontName || dwSubstSans == dwSysFontName) {
      nFind += FPF_SKIAMATCHWEIGHT_NAME2;
      bMatchedName = true;
    }
    if (uCharset == FX_Charset::kDefault || bMaybeSymbol) {
      if (nFind > nMax && bMatchedName) {
        nMax = nFind;
        pBestFont = font.get();
      }
    } else if (FPF_SkiaIsCJK(uCharset)) {
      if (bMatchedName || font->glyph_num() > nGlyphNum) {
        pBestFont = font.get();
        nGlyphNum = font->glyph_num();
      }
    } else if (nFind > nMax) {
      nMax = nFind;
      pBestFont = font.get();
    }
    if (nExpectVal <= nFind) {
      pBestFont = font.get();
      break;
    }
  }
  if (!pBestFont)
    return nullptr;

  auto font =
      std::make_unique<CFPF_SkiaFont>(this, pBestFont, dwStyle, uCharset);
  if (!font->IsValid())
    return nullptr;

  CFPF_SkiaFont* ret = font.get();
  m_FamilyFonts[dwHash] = std::move(font);
  return ret;
}

RetainPtr<CFX_Face> CFPF_SkiaFontMgr::GetFontFace(ByteStringView bsFile,
                                                  int32_t iFaceIndex) {
  if (bsFile.IsEmpty())
    return nullptr;

  if (iFaceIndex < 0)
    return nullptr;

  FT_Open_Args args;
  args.flags = FT_OPEN_PATHNAME;
  args.pathname = const_cast<FT_String*>(bsFile.unterminated_c_str());
  RetainPtr<CFX_Face> face =
      CFX_Face::Open(m_FTLibrary.get(), &args, iFaceIndex);
  if (!face)
    return nullptr;

  FT_Set_Pixel_Sizes(face->GetRec(), 0, 64);
  return face;
}

void CFPF_SkiaFontMgr::ScanPath(const ByteString& path) {
  std::unique_ptr<FX_Folder> handle = FX_Folder::OpenFolder(path);
  if (!handle)
    return;

  ByteString filename;
  bool bFolder = false;
  while (handle->GetNextFile(&filename, &bFolder)) {
    if (bFolder) {
      if (filename == "." || filename == "..")
        continue;
    } else {
      ByteString ext = filename.Last(4);
      ext.MakeLower();
      if (ext != ".ttf" && ext != ".ttc" && ext != ".otf")
        continue;
    }
    ByteString fullpath(path);
    fullpath += "/";
    fullpath += filename;
    if (bFolder)
      ScanPath(fullpath);
    else
      ScanFile(fullpath);
  }
}

void CFPF_SkiaFontMgr::ScanFile(const ByteString& file) {
  RetainPtr<CFX_Face> face = GetFontFace(file.AsStringView(), 0);
  if (!face)
    return;

  m_FontFaces.push_back(ReportFace(face, file));
}

std::unique_ptr<CFPF_SkiaPathFont> CFPF_SkiaFontMgr::ReportFace(
    RetainPtr<CFX_Face> face,
    const ByteString& file) {
  uint32_t dwStyle = 0;
  if (FXFT_Is_Face_Bold(face->GetRec()))
    dwStyle |= FXFONT_FORCE_BOLD;
  if (FXFT_Is_Face_Italic(face->GetRec()))
    dwStyle |= FXFONT_ITALIC;
  if (FT_IS_FIXED_WIDTH(face->GetRec()))
    dwStyle |= FXFONT_FIXED_PITCH;
  TT_OS2* pOS2 =
      static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face->GetRec(), ft_sfnt_os2));
  if (pOS2) {
    if (pOS2->ulCodePageRange1 & (1 << 31))
      dwStyle |= FXFONT_SYMBOLIC;
    if (pOS2->panose[0] == 2) {
      uint8_t uSerif = pOS2->panose[1];
      if ((uSerif > 1 && uSerif < 10) || uSerif > 13)
        dwStyle |= FXFONT_SERIF;
    }
  }
  if (pOS2 && (pOS2->ulCodePageRange1 & (1 << 31)))
    dwStyle |= FXFONT_SYMBOLIC;

  return std::make_unique<CFPF_SkiaPathFont>(
      file, FXFT_Get_Face_Family_Name(face->GetRec()), dwStyle,
      face->GetRec()->face_index, FPF_SkiaGetFaceCharset(pOS2),
      face->GetRec()->num_glyphs);
}
