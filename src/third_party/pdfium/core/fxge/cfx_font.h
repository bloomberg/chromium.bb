// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONT_H_
#define CORE_FXGE_CFX_FONT_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_codepage_forward.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/cfx_face.h"
#include "core/fxge/fx_freetype.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/base/span.h"

#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
#include "core/fxge/fx_font.h"
#endif

class CFX_GlyphBitmap;
class CFX_GlyphCache;
class CFX_Path;
class CFX_SubstFont;
class IFX_SeekableReadStream;
struct CFX_TextRenderOptions;

class CFX_Font {
 public:
  // This struct should be the same as FPDF_CharsetFontMap.
  struct CharsetFontMap {
    int charset;           // Character Set Enum value, see FX_Charset::kXXX.
    const char* fontname;  // Name of default font to use with that charset.
  };

  enum class FontType {
    kUnknown,
    kCIDTrueType,
  };

  // Pointer to the default character set to TT Font name map. The map is an
  // array of CharsetFontMap structs, with its end indicated by a {-1, nullptr}
  // entry.
  static const CharsetFontMap kDefaultTTFMap[];

  // Used when the font name is empty.
  static const char kUntitledFontName[];

  static const char kDefaultAnsiFontName[];
  static const char kUniversalDefaultFontName[];

  // Returns negative values on failure.
  static int GetWeightLevel(FX_Charset charset, size_t index);

  // |angle| is typically negative.
  static int GetSkewFromAngle(int angle);

  static ByteString GetDefaultFontNameByCharset(FX_Charset nCharset);
  static FX_Charset GetCharSetFromUnicode(uint16_t word);

  CFX_Font();
  ~CFX_Font();

  void LoadSubst(const ByteString& face_name,
                 bool bTrueType,
                 uint32_t flags,
                 int weight,
                 int italic_angle,
                 FX_CodePage code_page,
                 bool bVertical);

  bool LoadEmbedded(pdfium::span<const uint8_t> src_span,
                    bool force_vertical,
                    uint64_t object_tag);
  RetainPtr<CFX_Face> GetFace() const { return m_Face; }
  FXFT_FaceRec* GetFaceRec() const {
    return m_Face ? m_Face->GetRec() : nullptr;
  }
  CFX_SubstFont* GetSubstFont() const { return m_pSubstFont.get(); }
  int GetSubstFontItalicAngle() const;

#if defined(PDF_ENABLE_XFA)
  bool LoadFile(RetainPtr<IFX_SeekableReadStream> pFile, int nFaceIndex);

#if !BUILDFLAG(IS_WIN)
  void SetFace(RetainPtr<CFX_Face> face);
  void SetFontSpan(pdfium::span<uint8_t> pSpan) { m_FontData = pSpan; }
  void SetSubstFont(std::unique_ptr<CFX_SubstFont> subst);
#endif  // !BUILDFLAG(IS_WIN)
#endif  // defined(PDF_ENABLE_XFA)

  const CFX_GlyphBitmap* LoadGlyphBitmap(
      uint32_t glyph_index,
      bool bFontStyle,
      const CFX_Matrix& matrix,
      int dest_width,
      int anti_alias,
      CFX_TextRenderOptions* text_options) const;
  const CFX_Path* LoadGlyphPath(uint32_t glyph_index, int dest_width) const;
  int GetGlyphWidth(uint32_t glyph_index);
  int GetAscent() const;
  int GetDescent() const;
  absl::optional<FX_RECT> GetGlyphBBox(uint32_t glyph_index);
  bool IsItalic() const;
  bool IsBold() const;
  bool IsFixedWidth() const;
  bool IsVertical() const { return m_bVertical; }
  ByteString GetPsName() const;
  ByteString GetFamilyName() const;
  ByteString GetFaceName() const;
  ByteString GetBaseFontName() const;
  bool IsTTFont() const;

  // Raw bounding box.
  absl::optional<FX_RECT> GetRawBBox() const;

  // Bounding box adjusted for font units.
  absl::optional<FX_RECT> GetBBox() const;

  bool IsEmbedded() const { return m_bEmbedded; }
  uint8_t* GetSubData() const { return m_pGsubData.get(); }
  void SetSubData(uint8_t* data) { m_pGsubData.reset(data); }
  FontType GetFontType() const { return m_FontType; }
  void SetFontType(FontType type) { m_FontType = type; }
  uint64_t GetObjectTag() const { return m_ObjectTag; }
  pdfium::span<uint8_t> GetFontSpan() const { return m_FontData; }
  void AdjustMMParams(int glyph_index, int dest_width, int weight) const;
  std::unique_ptr<CFX_Path> LoadGlyphPathImpl(uint32_t glyph_index,
                                              int dest_width) const;

#if defined(_SKIA_SUPPORT_) || defined(_SKIA_SUPPORT_PATHS_)
  CFX_TypeFace* GetDeviceCache() const;
  bool IsSubstFontBold() const;
#endif

#if BUILDFLAG(IS_APPLE)
  void* GetPlatformFont() const { return m_pPlatformFont; }
  void SetPlatformFont(void* font) { m_pPlatformFont = font; }
#endif

 private:
  RetainPtr<CFX_GlyphCache> GetOrCreateGlyphCache() const;
  void ClearGlyphCache();
#if BUILDFLAG(IS_APPLE)
  void ReleasePlatformResource();
#endif
  ByteString GetFamilyNameOrUntitled() const;

#if defined(PDF_ENABLE_XFA)
  // |m_pOwnedFile| must outlive |m_pOwnedStreamRec|.
  RetainPtr<IFX_SeekableReadStream> m_pOwnedFile;
  std::unique_ptr<FXFT_StreamRec> m_pOwnedStreamRec;  // Must outlive |m_Face|.
#endif

  mutable RetainPtr<CFX_Face> m_Face;
  mutable RetainPtr<CFX_GlyphCache> m_GlyphCache;
  std::unique_ptr<CFX_SubstFont> m_pSubstFont;
  std::unique_ptr<uint8_t, FxFreeDeleter> m_pGsubData;
  std::vector<uint8_t, FxAllocAllocator<uint8_t>> m_FontDataAllocation;
  pdfium::span<uint8_t> m_FontData;
  FontType m_FontType = FontType::kUnknown;
  uint64_t m_ObjectTag = 0;
  bool m_bEmbedded = false;
  bool m_bVertical = false;
#if BUILDFLAG(IS_APPLE)
  void* m_pPlatformFont = nullptr;
#endif
};

#endif  // CORE_FXGE_CFX_FONT_H_
