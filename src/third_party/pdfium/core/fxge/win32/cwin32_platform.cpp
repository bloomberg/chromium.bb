// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/win32/cwin32_platform.h"

#include <iterator>
#include <memory>
#include <utility>

#include "core/fxcrt/fx_codepage.h"
#include "core/fxge/cfx_folderfontinfo.h"
#include "core/fxge/cfx_gemodule.h"
#include "third_party/base/numerics/safe_conversions.h"
#include "third_party/base/win/scoped_select_object.h"
#include "third_party/base/win/win_util.h"

namespace {

using ScopedSelectObject = pdfium::base::win::ScopedSelectObject;

struct Variant {
  const char* m_pFaceName;
  const char* m_pVariantName;  // Note: UTF-16LE terminator required.
};

constexpr Variant kVariantNames[] = {
    {"DFKai-SB", "\x19\x6A\x77\x69\xD4\x9A\x00\x00"},
};

struct Substs {
  const char* m_pName;
  const char* m_pWinName;
  bool m_bBold;
  bool m_bItalic;
};

constexpr Substs kBase14Substs[] = {
    {"Courier", "Courier New", false, false},
    {"Courier-Bold", "Courier New", true, false},
    {"Courier-BoldOblique", "Courier New", true, true},
    {"Courier-Oblique", "Courier New", false, true},
    {"Helvetica", "Arial", false, false},
    {"Helvetica-Bold", "Arial", true, false},
    {"Helvetica-BoldOblique", "Arial", true, true},
    {"Helvetica-Oblique", "Arial", false, true},
    {"Times-Roman", "Times New Roman", false, false},
    {"Times-Bold", "Times New Roman", true, false},
    {"Times-BoldItalic", "Times New Roman", true, true},
    {"Times-Italic", "Times New Roman", false, true},
};

struct FontNameMap {
  const char* m_pSubFontName;
  const char* m_pSrcFontName;
};

constexpr FontNameMap kJpFontNameMap[] = {
    {"MS Mincho", "Heiseimin-W3"},
    {"MS Gothic", "Jun101-Light"},
};

bool GetSubFontName(ByteString* name) {
  for (size_t i = 0; i < std::size(kJpFontNameMap); ++i) {
    if (!FXSYS_stricmp(name->c_str(), kJpFontNameMap[i].m_pSrcFontName)) {
      *name = kJpFontNameMap[i].m_pSubFontName;
      return true;
    }
  }
  return false;
}

class CFX_Win32FallbackFontInfo final : public CFX_FolderFontInfo {
 public:
  CFX_Win32FallbackFontInfo() = default;
  ~CFX_Win32FallbackFontInfo() override = default;

  // CFX_FolderFontInfo:
  void* MapFont(int weight,
                bool bItalic,
                FX_Charset charset,
                int pitch_family,
                const ByteString& face) override;
};

class CFX_Win32FontInfo final : public SystemFontInfoIface {
 public:
  CFX_Win32FontInfo();
  ~CFX_Win32FontInfo() override;

  // SystemFontInfoIface:
  bool EnumFontList(CFX_FontMapper* pMapper) override;
  void* MapFont(int weight,
                bool bItalic,
                FX_Charset charset,
                int pitch_family,
                const ByteString& face) override;
  void* GetFont(const ByteString& face) override { return nullptr; }
  size_t GetFontData(void* hFont,
                     uint32_t table,
                     pdfium::span<uint8_t> buffer) override;
  bool GetFaceName(void* hFont, ByteString* name) override;
  bool GetFontCharset(void* hFont, FX_Charset* charset) override;
  void DeleteFont(void* hFont) override;

  void AddInstalledFont(const LOGFONTA* plf, uint32_t font_type);

 private:
  bool IsSupportedFont(const LOGFONTA* plf);
  void GetGBPreference(ByteString& face, int weight, int pitch_family);
  void GetJapanesePreference(ByteString& face, int weight, int pitch_family);
  ByteString FindFont(const ByteString& name);

  const HDC m_hDC;
  UnownedPtr<CFX_FontMapper> m_pMapper;
  ByteString m_LastFamily;
  ByteString m_KaiTi;
  ByteString m_FangSong;
};

int CALLBACK FontEnumProc(const LOGFONTA* plf,
                          const TEXTMETRICA* lpntme,
                          uint32_t font_type,
                          LPARAM lParam) {
  CFX_Win32FontInfo* pFontInfo = reinterpret_cast<CFX_Win32FontInfo*>(lParam);
  pFontInfo->AddInstalledFont(plf, font_type);
  return 1;
}

CFX_Win32FontInfo::CFX_Win32FontInfo() : m_hDC(CreateCompatibleDC(nullptr)) {}

CFX_Win32FontInfo::~CFX_Win32FontInfo() {
  DeleteDC(m_hDC);
}

bool CFX_Win32FontInfo::IsSupportedFont(const LOGFONTA* plf) {
  HFONT hFont = CreateFontIndirectA(plf);
  bool ret = false;
  size_t font_size = GetFontData(hFont, 0, {});
  if (font_size != GDI_ERROR && font_size >= sizeof(uint32_t)) {
    uint32_t header;
    auto span = pdfium::as_writable_bytes(pdfium::make_span(&header, 1));
    GetFontData(hFont, 0, span);
    header = FXSYS_UINT32_GET_MSBFIRST(span);
    ret = header == FXBSTR_ID('O', 'T', 'T', 'O') ||
          header == FXBSTR_ID('t', 't', 'c', 'f') ||
          header == FXBSTR_ID('t', 'r', 'u', 'e') || header == 0x00010000 ||
          header == 0x00020000 ||
          (header & 0xFFFF0000) == FXBSTR_ID(0x80, 0x01, 0x00, 0x00) ||
          (header & 0xFFFF0000) == FXBSTR_ID('%', '!', 0, 0);
  }
  DeleteFont(hFont);
  return ret;
}

void CFX_Win32FontInfo::AddInstalledFont(const LOGFONTA* plf,
                                         uint32_t font_type) {
  ByteString name(plf->lfFaceName);
  if (name.GetLength() > 0 && name[0] == '@')
    return;

  if (name == m_LastFamily) {
    m_pMapper->AddInstalledFont(name, FX_GetCharsetFromInt(plf->lfCharSet));
    return;
  }
  if (!(font_type & TRUETYPE_FONTTYPE)) {
    if (!(font_type & DEVICE_FONTTYPE) || !IsSupportedFont(plf))
      return;
  }

  m_pMapper->AddInstalledFont(name, FX_GetCharsetFromInt(plf->lfCharSet));
  m_LastFamily = name;
}

bool CFX_Win32FontInfo::EnumFontList(CFX_FontMapper* pMapper) {
  m_pMapper = pMapper;
  LOGFONTA lf;
  memset(&lf, 0, sizeof(LOGFONTA));
  lf.lfCharSet = static_cast<int>(FX_Charset::kDefault);
  lf.lfFaceName[0] = 0;
  lf.lfPitchAndFamily = 0;
  EnumFontFamiliesExA(m_hDC, &lf, reinterpret_cast<FONTENUMPROCA>(FontEnumProc),
                      reinterpret_cast<uintptr_t>(this), 0);
  return true;
}

ByteString CFX_Win32FontInfo::FindFont(const ByteString& name) {
  if (!m_pMapper)
    return name;

  absl::optional<ByteString> maybe_installed =
      m_pMapper->InstalledFontNameStartingWith(name);
  if (maybe_installed.has_value())
    return maybe_installed.value();

  absl::optional<ByteString> maybe_localized =
      m_pMapper->LocalizedFontNameStartingWith(name);
  if (maybe_localized.has_value())
    return maybe_localized.value();

  return ByteString();
}

void* CFX_Win32FallbackFontInfo::MapFont(int weight,
                                         bool bItalic,
                                         FX_Charset charset,
                                         int pitch_family,
                                         const ByteString& face) {
  void* font = GetSubstFont(face);
  if (font)
    return font;

  bool bCJK = true;
  switch (charset) {
    case FX_Charset::kShiftJIS:
    case FX_Charset::kChineseSimplified:
    case FX_Charset::kChineseTraditional:
    case FX_Charset::kHangul:
      break;
    default:
      bCJK = false;
      break;
  }
  return FindFont(weight, bItalic, charset, pitch_family, face, !bCJK);
}

void CFX_Win32FontInfo::GetGBPreference(ByteString& face,
                                        int weight,
                                        int pitch_family) {
  if (face.Contains("KaiTi") || face.Contains("\xbf\xac")) {
    if (m_KaiTi.IsEmpty()) {
      m_KaiTi = FindFont("KaiTi");
      if (m_KaiTi.IsEmpty()) {
        m_KaiTi = "SimSun";
      }
    }
    face = m_KaiTi;
  } else if (face.Contains("FangSong") || face.Contains("\xb7\xc2\xcb\xce")) {
    if (m_FangSong.IsEmpty()) {
      m_FangSong = FindFont("FangSong");
      if (m_FangSong.IsEmpty()) {
        m_FangSong = "SimSun";
      }
    }
    face = m_FangSong;
  } else if (face.Contains("SimSun") || face.Contains("\xcb\xce")) {
    face = "SimSun";
  } else if (face.Contains("SimHei") || face.Contains("\xba\xda")) {
    face = "SimHei";
  } else if (!(pitch_family & FF_ROMAN) && weight > 550) {
    face = "SimHei";
  } else {
    face = "SimSun";
  }
}

void CFX_Win32FontInfo::GetJapanesePreference(ByteString& face,
                                              int weight,
                                              int pitch_family) {
  if (face.Contains("Gothic") ||
      face.Contains("\x83\x53\x83\x56\x83\x62\x83\x4e")) {
    if (face.Contains("PGothic") ||
        face.Contains("\x82\x6f\x83\x53\x83\x56\x83\x62\x83\x4e")) {
      face = "MS PGothic";
    } else if (face.Contains("UI Gothic")) {
      face = "MS UI Gothic";
    } else {
      if (face.Contains("HGSGothicM") || face.Contains("HGMaruGothicMPRO")) {
        face = "MS PGothic";
      } else {
        face = "MS Gothic";
      }
    }
    return;
  }
  if (face.Contains("Mincho") || face.Contains("\x96\xbe\x92\xa9")) {
    if (face.Contains("PMincho") || face.Contains("\x82\x6f\x96\xbe\x92\xa9")) {
      face = "MS PMincho";
    } else {
      face = "MS Mincho";
    }
    return;
  }
  if (GetSubFontName(&face))
    return;

  if (!(pitch_family & FF_ROMAN) && weight > 400) {
    face = "MS PGothic";
  } else {
    face = "MS PMincho";
  }
}

void* CFX_Win32FontInfo::MapFont(int weight,
                                 bool bItalic,
                                 FX_Charset charset,
                                 int pitch_family,
                                 const ByteString& face) {
  ByteString new_face = face;
  for (int iBaseFont = 0; iBaseFont < 12; iBaseFont++) {
    if (new_face == ByteStringView(kBase14Substs[iBaseFont].m_pName)) {
      new_face = kBase14Substs[iBaseFont].m_pWinName;
      weight = kBase14Substs[iBaseFont].m_bBold ? FW_BOLD : FW_NORMAL;
      bItalic = kBase14Substs[iBaseFont].m_bItalic;
      break;
    }
  }
  if (charset == FX_Charset::kANSI || charset == FX_Charset::kSymbol)
    charset = FX_Charset::kDefault;

  int subst_pitch_family = pitch_family;
  switch (charset) {
    case FX_Charset::kShiftJIS:
      subst_pitch_family = FF_ROMAN;
      break;
    case FX_Charset::kChineseTraditional:
    case FX_Charset::kHangul:
    case FX_Charset::kChineseSimplified:
      subst_pitch_family = 0;
      break;
  }
  HFONT hFont = ::CreateFontA(-10, 0, 0, 0, weight, bItalic, 0, 0,
                              static_cast<int>(charset), OUT_TT_ONLY_PRECIS, 0,
                              0, subst_pitch_family, new_face.c_str());
  char facebuf[100];
  {
    ScopedSelectObject select_object(m_hDC, hFont);
    ::GetTextFaceA(m_hDC, std::size(facebuf), facebuf);
  }
  if (new_face.EqualNoCase(facebuf))
    return hFont;

  WideString wsFace = WideString::FromDefANSI(facebuf);
  for (size_t i = 0; i < std::size(kVariantNames); ++i) {
    if (new_face != kVariantNames[i].m_pFaceName)
      continue;

    const unsigned short* pName = reinterpret_cast<const unsigned short*>(
        kVariantNames[i].m_pVariantName);
    size_t len = WideString::WStringLength(pName);
    WideString wsName = WideString::FromUTF16LE(pName, len);
    if (wsFace == wsName)
      return hFont;
  }
  ::DeleteObject(hFont);
  if (charset == FX_Charset::kDefault)
    return nullptr;

  switch (charset) {
    case FX_Charset::kShiftJIS:
      GetJapanesePreference(new_face, weight, pitch_family);
      break;
    case FX_Charset::kChineseSimplified:
      GetGBPreference(new_face, weight, pitch_family);
      break;
    case FX_Charset::kHangul:
      new_face = "Gulim";
      break;
    case FX_Charset::kChineseTraditional:
      if (new_face.Contains("MSung")) {
        new_face = "MingLiU";
      } else {
        new_face = "PMingLiU";
      }
      break;
  }
  hFont = ::CreateFontA(-10, 0, 0, 0, weight, bItalic, 0, 0,
                        static_cast<int>(charset), OUT_TT_ONLY_PRECIS, 0, 0,
                        subst_pitch_family, new_face.c_str());
  return hFont;
}

void CFX_Win32FontInfo::DeleteFont(void* hFont) {
  ::DeleteObject(hFont);
}

size_t CFX_Win32FontInfo::GetFontData(void* hFont,
                                      uint32_t table,
                                      pdfium::span<uint8_t> buffer) {
  ScopedSelectObject select_object(m_hDC, static_cast<HFONT>(hFont));
  table = FXSYS_UINT32_GET_MSBFIRST(reinterpret_cast<uint8_t*>(&table));
  size_t size = ::GetFontData(m_hDC, table, 0, buffer.data(),
                              pdfium::base::checked_cast<DWORD>(buffer.size()));
  return size != GDI_ERROR ? size : 0;
}

bool CFX_Win32FontInfo::GetFaceName(void* hFont, ByteString* name) {
  ScopedSelectObject select_object(m_hDC, static_cast<HFONT>(hFont));
  char facebuf[100];
  if (::GetTextFaceA(m_hDC, 100, facebuf) == 0)
    return false;

  *name = facebuf;
  return true;
}

bool CFX_Win32FontInfo::GetFontCharset(void* hFont, FX_Charset* charset) {
  ScopedSelectObject select_object(m_hDC, static_cast<HFONT>(hFont));
  TEXTMETRIC tm;
  ::GetTextMetrics(m_hDC, &tm);
  *charset = FX_GetCharsetFromInt(tm.tmCharSet);
  return true;
}

}  // namespace

CWin32Platform::CWin32Platform() = default;

CWin32Platform::~CWin32Platform() = default;

void CWin32Platform::Init() {
  if (pdfium::base::win::IsUser32AndGdi32Available())
    m_GdiplusExt.Load();
}

std::unique_ptr<SystemFontInfoIface>
CWin32Platform::CreateDefaultSystemFontInfo() {
  auto** user_paths = CFX_GEModule::Get()->GetUserFontPaths();
  if (user_paths) {
    auto font_info = std::make_unique<CFX_Win32FallbackFontInfo>();
    for (; *user_paths; user_paths++)
      font_info->AddPath(*user_paths);
    return std::move(font_info);
  }

  if (pdfium::base::win::IsUser32AndGdi32Available())
    return std::make_unique<CFX_Win32FontInfo>();

  // Select the fallback font information class if GDI is disabled.
  auto fallback_info = std::make_unique<CFX_Win32FallbackFontInfo>();
  // Construct the font path manually, SHGetKnownFolderPath won't work under
  // a restrictive sandbox.
  CHAR windows_path[MAX_PATH] = {};
  DWORD path_len = ::GetWindowsDirectoryA(windows_path, MAX_PATH);
  if (path_len > 0 && path_len < MAX_PATH) {
    ByteString fonts_path(windows_path);
    fonts_path += "\\Fonts";
    fallback_info->AddPath(fonts_path);
  }
  return fallback_info;
}

// static
std::unique_ptr<CFX_GEModule::PlatformIface>
CFX_GEModule::PlatformIface::Create() {
  return std::make_unique<CWin32Platform>();
}
