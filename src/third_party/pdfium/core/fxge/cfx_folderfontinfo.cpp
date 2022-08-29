// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_folderfontinfo.h"

#include <iterator>
#include <limits>
#include <utility>

#include "build/build_config.h"
#include "core/fxcrt/fx_codepage.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_folder.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/fx_safe_types.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/fx_font.h"
#include "third_party/base/containers/contains.h"

namespace {

const struct {
  const char* m_pName;
  const char* m_pSubstName;
} Base14Substs[] = {
    {"Courier", "Courier New"},
    {"Courier-Bold", "Courier New Bold"},
    {"Courier-BoldOblique", "Courier New Bold Italic"},
    {"Courier-Oblique", "Courier New Italic"},
    {"Helvetica", "Arial"},
    {"Helvetica-Bold", "Arial Bold"},
    {"Helvetica-BoldOblique", "Arial Bold Italic"},
    {"Helvetica-Oblique", "Arial Italic"},
    {"Times-Roman", "Times New Roman"},
    {"Times-Bold", "Times New Roman Bold"},
    {"Times-BoldItalic", "Times New Roman Bold Italic"},
    {"Times-Italic", "Times New Roman Italic"},
};

// Used with std::unique_ptr to automatically call fclose().
struct FxFileCloser {
  inline void operator()(FILE* h) const {
    if (h)
      fclose(h);
  }
};

bool FindFamilyNameMatch(ByteStringView family_name,
                         const ByteString& installed_font_name) {
  absl::optional<size_t> result = installed_font_name.Find(family_name, 0);
  if (!result.has_value())
    return false;

  size_t next_index = result.value() + family_name.GetLength();
  // Rule out the case that |family_name| is a substring of
  // |installed_font_name| but their family names are actually different words.
  // For example: "Univers" and "Universal" are not a match because they have
  // different family names, but "Univers" and "Univers Bold" are a match.
  if (installed_font_name.IsValidIndex(next_index) &&
      FXSYS_IsLowerASCII(installed_font_name[next_index])) {
    return false;
  }

  return true;
}

ByteString ReadStringFromFile(FILE* pFile, uint32_t size) {
  ByteString result;
  {
    // Span's lifetime must end before ReleaseBuffer() below.
    pdfium::span<char> buffer = result.GetBuffer(size);
    if (!fread(buffer.data(), size, 1, pFile))
      return ByteString();
  }
  result.ReleaseBuffer(size);
  return result;
}

ByteString LoadTableFromTT(FILE* pFile,
                           const uint8_t* pTables,
                           uint32_t nTables,
                           uint32_t tag,
                           FX_FILESIZE fileSize) {
  for (uint32_t i = 0; i < nTables; i++) {
    const uint8_t* p = pTables + i * 16;
    if (FXSYS_UINT32_GET_MSBFIRST(p) == tag) {
      uint32_t offset = FXSYS_UINT32_GET_MSBFIRST(p + 8);
      uint32_t size = FXSYS_UINT32_GET_MSBFIRST(p + 12);
      if (offset > std::numeric_limits<uint32_t>::max() - size ||
          static_cast<FX_FILESIZE>(offset + size) > fileSize ||
          fseek(pFile, offset, SEEK_SET) < 0) {
        return ByteString();
      }
      return ReadStringFromFile(pFile, size);
    }
  }
  return ByteString();
}

uint32_t GetCharset(FX_Charset charset) {
  switch (charset) {
    case FX_Charset::kShiftJIS:
      return CHARSET_FLAG_SHIFTJIS;
    case FX_Charset::kChineseSimplified:
      return CHARSET_FLAG_GB;
    case FX_Charset::kChineseTraditional:
      return CHARSET_FLAG_BIG5;
    case FX_Charset::kHangul:
      return CHARSET_FLAG_KOREAN;
    case FX_Charset::kSymbol:
      return CHARSET_FLAG_SYMBOL;
    case FX_Charset::kANSI:
      return CHARSET_FLAG_ANSI;
    default:
      break;
  }
  return 0;
}

int32_t GetSimilarValue(int weight,
                        bool bItalic,
                        int pitch_family,
                        uint32_t style,
                        bool bMatchName,
                        size_t familyNameLength,
                        size_t bsNameLength) {
  int32_t iSimilarValue = 0;
  if (bMatchName && (familyNameLength == bsNameLength))
    iSimilarValue += 4;
  if (FontStyleIsForceBold(style) == (weight > 400))
    iSimilarValue += 16;
  if (FontStyleIsItalic(style) == bItalic)
    iSimilarValue += 16;
  if (FontStyleIsSerif(style) == FontFamilyIsRoman(pitch_family))
    iSimilarValue += 16;
  if (FontStyleIsScript(style) == FontFamilyIsScript(pitch_family))
    iSimilarValue += 8;
  if (FontStyleIsFixedPitch(style) == FontFamilyIsFixedPitch(pitch_family))
    iSimilarValue += 8;
  return iSimilarValue;
}

}  // namespace

CFX_FolderFontInfo::CFX_FolderFontInfo() = default;

CFX_FolderFontInfo::~CFX_FolderFontInfo() = default;

void CFX_FolderFontInfo::AddPath(const ByteString& path) {
  m_PathList.push_back(path);
}

bool CFX_FolderFontInfo::EnumFontList(CFX_FontMapper* pMapper) {
  m_pMapper = pMapper;
  for (const auto& path : m_PathList)
    ScanPath(path);
  return true;
}

void CFX_FolderFontInfo::ScanPath(const ByteString& path) {
  std::unique_ptr<FX_Folder> handle = FX_Folder::OpenFolder(path);
  if (!handle)
    return;

  ByteString filename;
  bool bFolder;
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

    ByteString fullpath = path;
#if BUILDFLAG(IS_WIN)
    fullpath += "\\";
#else
    fullpath += "/";
#endif

    fullpath += filename;
    bFolder ? ScanPath(fullpath) : ScanFile(fullpath);
  }
}

void CFX_FolderFontInfo::ScanFile(const ByteString& path) {
  std::unique_ptr<FILE, FxFileCloser> pFile(fopen(path.c_str(), "rb"));
  if (!pFile)
    return;

  fseek(pFile.get(), 0, SEEK_END);

  FX_FILESIZE filesize = ftell(pFile.get());
  uint8_t buffer[16];
  fseek(pFile.get(), 0, SEEK_SET);

  size_t readCnt = fread(buffer, 12, 1, pFile.get());
  if (readCnt != 1)
    return;

  if (FXSYS_UINT32_GET_MSBFIRST(buffer) != kTableTTCF) {
    ReportFace(path, pFile.get(), filesize, 0);
    return;
  }

  uint32_t nFaces = FXSYS_UINT32_GET_MSBFIRST(buffer + 8);
  FX_SAFE_SIZE_T safe_face_bytes = nFaces;
  safe_face_bytes *= 4;
  if (!safe_face_bytes.IsValid())
    return;

  const size_t face_bytes = safe_face_bytes.ValueOrDie();
  std::unique_ptr<uint8_t, FxFreeDeleter> offsets(
      FX_Alloc(uint8_t, face_bytes));
  readCnt = fread(offsets.get(), 1, face_bytes, pFile.get());
  if (readCnt != face_bytes)
    return;

  auto offsets_span = pdfium::make_span(offsets.get(), face_bytes);
  for (uint32_t i = 0; i < nFaces; i++) {
    ReportFace(path, pFile.get(), filesize,
               FXSYS_UINT32_GET_MSBFIRST(&offsets_span[i * 4]));
  }
}

void CFX_FolderFontInfo::ReportFace(const ByteString& path,
                                    FILE* pFile,
                                    FX_FILESIZE filesize,
                                    uint32_t offset) {
  char buffer[16];
  if (fseek(pFile, offset, SEEK_SET) < 0 || !fread(buffer, 12, 1, pFile))
    return;

  uint32_t nTables = FXSYS_UINT16_GET_MSBFIRST(buffer + 4);
  ByteString tables = ReadStringFromFile(pFile, nTables * 16);
  if (tables.IsEmpty())
    return;

  static constexpr uint32_t kNameTag =
      CFX_FontMapper::MakeTag('n', 'a', 'm', 'e');
  ByteString names =
      LoadTableFromTT(pFile, tables.raw_str(), nTables, kNameTag, filesize);
  if (names.IsEmpty())
    return;

  ByteString facename = GetNameFromTT(names.raw_span(), 1);
  if (facename.IsEmpty())
    return;

  ByteString style = GetNameFromTT(names.raw_span(), 2);
  if (style != "Regular")
    facename += " " + style;

  if (pdfium::Contains(m_FontList, facename))
    return;

  auto pInfo =
      std::make_unique<FontFaceInfo>(path, facename, tables, offset, filesize);
  static constexpr uint32_t kOs2Tag =
      CFX_FontMapper::MakeTag('O', 'S', '/', '2');
  ByteString os2 =
      LoadTableFromTT(pFile, tables.raw_str(), nTables, kOs2Tag, filesize);
  if (os2.GetLength() >= 86) {
    const uint8_t* p = os2.raw_str() + 78;
    uint32_t codepages = FXSYS_UINT32_GET_MSBFIRST(p);
    if (codepages & (1U << 17)) {
      m_pMapper->AddInstalledFont(facename, FX_Charset::kShiftJIS);
      pInfo->m_Charsets |= CHARSET_FLAG_SHIFTJIS;
    }
    if (codepages & (1U << 18)) {
      m_pMapper->AddInstalledFont(facename, FX_Charset::kChineseSimplified);
      pInfo->m_Charsets |= CHARSET_FLAG_GB;
    }
    if (codepages & (1U << 20)) {
      m_pMapper->AddInstalledFont(facename, FX_Charset::kChineseTraditional);
      pInfo->m_Charsets |= CHARSET_FLAG_BIG5;
    }
    if ((codepages & (1U << 19)) || (codepages & (1U << 21))) {
      m_pMapper->AddInstalledFont(facename, FX_Charset::kHangul);
      pInfo->m_Charsets |= CHARSET_FLAG_KOREAN;
    }
    if (codepages & (1U << 31)) {
      m_pMapper->AddInstalledFont(facename, FX_Charset::kSymbol);
      pInfo->m_Charsets |= CHARSET_FLAG_SYMBOL;
    }
  }
  m_pMapper->AddInstalledFont(facename, FX_Charset::kANSI);
  pInfo->m_Charsets |= CHARSET_FLAG_ANSI;
  pInfo->m_Styles = 0;
  if (style.Contains("Bold"))
    pInfo->m_Styles |= FXFONT_FORCE_BOLD;
  if (style.Contains("Italic") || style.Contains("Oblique"))
    pInfo->m_Styles |= FXFONT_ITALIC;
  if (facename.Contains("Serif"))
    pInfo->m_Styles |= FXFONT_SERIF;

  m_FontList[facename] = std::move(pInfo);
}

void* CFX_FolderFontInfo::GetSubstFont(const ByteString& face) {
  for (size_t iBaseFont = 0; iBaseFont < std::size(Base14Substs); iBaseFont++) {
    if (face == Base14Substs[iBaseFont].m_pName)
      return GetFont(Base14Substs[iBaseFont].m_pSubstName);
  }
  return nullptr;
}

void* CFX_FolderFontInfo::FindFont(int weight,
                                   bool bItalic,
                                   FX_Charset charset,
                                   int pitch_family,
                                   const ByteString& family,
                                   bool bMatchName) {
  FontFaceInfo* pFind = nullptr;
  if (charset == FX_Charset::kANSI && FontFamilyIsFixedPitch(pitch_family)) {
    auto* courier_new = GetFont("Courier New");
    if (courier_new)
      return courier_new;
  }

  ByteStringView bsFamily = family.AsStringView();
  uint32_t charset_flag = GetCharset(charset);
  int32_t iBestSimilar = 0;
  for (const auto& it : m_FontList) {
    const ByteString& bsName = it.first;
    FontFaceInfo* pFont = it.second.get();
    if (!(pFont->m_Charsets & charset_flag) && charset != FX_Charset::kDefault)
      continue;

    if (bMatchName && !FindFamilyNameMatch(bsFamily, bsName))
      continue;

    int32_t iSimilarValue =
        GetSimilarValue(weight, bItalic, pitch_family, pFont->m_Styles,
                        bMatchName, bsFamily.GetLength(), bsName.GetLength());
    if (iSimilarValue > iBestSimilar) {
      iBestSimilar = iSimilarValue;
      pFind = pFont;
    }
  }
  return pFind;
}

void* CFX_FolderFontInfo::MapFont(int weight,
                                  bool bItalic,
                                  FX_Charset charset,
                                  int pitch_family,
                                  const ByteString& face) {
  return nullptr;
}

void* CFX_FolderFontInfo::GetFont(const ByteString& face) {
  auto it = m_FontList.find(face);
  return it != m_FontList.end() ? it->second.get() : nullptr;
}

size_t CFX_FolderFontInfo::GetFontData(void* hFont,
                                       uint32_t table,
                                       pdfium::span<uint8_t> buffer) {
  if (!hFont)
    return 0;

  const FontFaceInfo* pFont = static_cast<FontFaceInfo*>(hFont);
  uint32_t datasize = 0;
  uint32_t offset = 0;
  if (table == 0) {
    datasize = pFont->m_FontOffset ? 0 : pFont->m_FileSize;
  } else if (table == kTableTTCF) {
    datasize = pFont->m_FontOffset ? pFont->m_FileSize : 0;
  } else {
    size_t nTables = pFont->m_FontTables.GetLength() / 16;
    for (size_t i = 0; i < nTables; i++) {
      const uint8_t* p = pFont->m_FontTables.raw_str() + i * 16;
      if (FXSYS_UINT32_GET_MSBFIRST(p) == table) {
        offset = FXSYS_UINT32_GET_MSBFIRST(p + 8);
        datasize = FXSYS_UINT32_GET_MSBFIRST(p + 12);
      }
    }
  }

  if (!datasize || buffer.size() < datasize)
    return datasize;

  std::unique_ptr<FILE, FxFileCloser> pFile(
      fopen(pFont->m_FilePath.c_str(), "rb"));
  if (!pFile)
    return 0;

  if (fseek(pFile.get(), offset, SEEK_SET) < 0 ||
      fread(buffer.data(), datasize, 1, pFile.get()) != 1) {
    return 0;
  }
  return datasize;
}

void CFX_FolderFontInfo::DeleteFont(void* hFont) {}

bool CFX_FolderFontInfo::GetFaceName(void* hFont, ByteString* name) {
  if (!hFont)
    return false;
  *name = static_cast<FontFaceInfo*>(hFont)->m_FaceName;
  return true;
}

bool CFX_FolderFontInfo::GetFontCharset(void* hFont, FX_Charset* charset) {
  return false;
}

CFX_FolderFontInfo::FontFaceInfo::FontFaceInfo(ByteString filePath,
                                               ByteString faceName,
                                               ByteString fontTables,
                                               uint32_t fontOffset,
                                               uint32_t fileSize)
    : m_FilePath(filePath),
      m_FaceName(faceName),
      m_FontTables(fontTables),
      m_FontOffset(fontOffset),
      m_FileSize(fileSize) {}
