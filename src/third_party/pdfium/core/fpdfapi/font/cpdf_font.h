// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_FONT_H_
#define CORE_FPDFAPI_FONT_CPDF_FONT_H_

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_font.h"

class CFX_DIBitmap;
class CFX_SubstFont;
class CPDF_CIDFont;
class CPDF_Document;
class CPDF_Object;
class CPDF_TrueTypeFont;
class CPDF_Type1Font;
class CPDF_Type3Char;
class CPDF_Type3Font;
class CPDF_ToUnicodeMap;

class CPDF_Font : public Retainable, public Observable {
 public:
  // Callback mechanism for Type3 fonts to get pixels from forms.
  class FormIface {
   public:
    virtual ~FormIface() {}

    virtual void ParseContentForType3Char(CPDF_Type3Char* pChar) = 0;
    virtual bool HasPageObjects() const = 0;
    virtual CFX_FloatRect CalcBoundingBox() const = 0;
    virtual Optional<std::pair<RetainPtr<CFX_DIBitmap>, CFX_Matrix>>
    GetBitmapAndMatrixFromSoleImageOfForm() const = 0;
  };

  // Callback mechanism for Type3 fonts to get new forms from upper layers.
  class FormFactoryIface {
   public:
    virtual ~FormFactoryIface() {}

    virtual std::unique_ptr<FormIface> CreateForm(
        CPDF_Document* pDocument,
        CPDF_Dictionary* pPageResources,
        CPDF_Stream* pFormStream) = 0;
  };

  static constexpr uint32_t kInvalidCharCode = static_cast<uint32_t>(-1);

  // |pFactory| only required for Type3 fonts.
  static RetainPtr<CPDF_Font> Create(CPDF_Document* pDoc,
                                     CPDF_Dictionary* pFontDict,
                                     FormFactoryIface* pFactory);
  static RetainPtr<CPDF_Font> GetStockFont(CPDF_Document* pDoc,
                                           ByteStringView fontname);

  ~CPDF_Font() override;

  virtual bool IsType1Font() const;
  virtual bool IsTrueTypeFont() const;
  virtual bool IsType3Font() const;
  virtual bool IsCIDFont() const;
  virtual const CPDF_Type1Font* AsType1Font() const;
  virtual CPDF_Type1Font* AsType1Font();
  virtual const CPDF_TrueTypeFont* AsTrueTypeFont() const;
  virtual CPDF_TrueTypeFont* AsTrueTypeFont();
  virtual const CPDF_Type3Font* AsType3Font() const;
  virtual CPDF_Type3Font* AsType3Font();
  virtual const CPDF_CIDFont* AsCIDFont() const;
  virtual CPDF_CIDFont* AsCIDFont();

  virtual void WillBeDestroyed();
  virtual bool IsVertWriting() const;
  virtual bool IsUnicodeCompatible() const;
  virtual uint32_t GetNextChar(ByteStringView pString, size_t* pOffset) const;
  virtual size_t CountChar(ByteStringView pString) const;
  virtual int AppendChar(char* buf, uint32_t charcode) const;
  virtual int GlyphFromCharCode(uint32_t charcode, bool* pVertGlyph) = 0;
#if defined(OS_MACOSX)
  virtual int GlyphFromCharCodeExt(uint32_t charcode);
#endif
  virtual WideString UnicodeFromCharCode(uint32_t charcode) const;
  virtual uint32_t CharCodeFromUnicode(wchar_t Unicode) const;
  virtual bool HasFontWidths() const;

  ByteString GetBaseFontName() const { return m_BaseFontName; }
  CFX_SubstFont* GetSubstFont() const { return m_Font.GetSubstFont(); }
  bool IsEmbedded() const { return IsType3Font() || m_pFontFile != nullptr; }
  CPDF_Dictionary* GetFontDict() const { return m_pFontDict.Get(); }
  void ClearFontDict() { m_pFontDict = nullptr; }
  bool IsStandardFont() const;
  bool HasFace() const { return !!m_Font.GetFaceRec(); }
  void AppendChar(ByteString* str, uint32_t charcode) const;

  const FX_RECT& GetFontBBox() const { return m_FontBBox; }
  int GetTypeAscent() const { return m_Ascent; }
  int GetTypeDescent() const { return m_Descent; }
  uint32_t GetStringWidth(ByteStringView pString);
  uint32_t FallbackFontFromCharcode(uint32_t charcode);
  int FallbackGlyphFromCharcode(int fallbackFont, uint32_t charcode);
  int GetFontFlags() const { return m_Flags; }
  int GetFontWeight() const;

  virtual uint32_t GetCharWidthF(uint32_t charcode) = 0;
  virtual FX_RECT GetCharBBox(uint32_t charcode) = 0;

  // Can return nullptr for stock Type1 fonts. Always returns non-null for other
  // font types.
  CPDF_Document* GetDocument() const { return m_pDocument.Get(); }

  CFX_Font* GetFont() { return &m_Font; }
  const CFX_Font* GetFont() const { return &m_Font; }

  CFX_Font* GetFontFallback(int position);

 protected:
  CPDF_Font(CPDF_Document* pDocument, CPDF_Dictionary* pFontDict);

  static int TT2PDF(int m, FXFT_FaceRec* face);
  static bool FT_UseTTCharmap(FXFT_FaceRec* face,
                              int platform_id,
                              int encoding_id);
  static const char* GetAdobeCharName(int iBaseEncoding,
                                      const std::vector<ByteString>& charnames,
                                      uint32_t charcode);

  virtual bool Load() = 0;

  void LoadUnicodeMap() const;  // logically const only.
  void LoadFontDescriptor(const CPDF_Dictionary* pFontDesc);
  void CheckFontMetrics();

  UnownedPtr<CPDF_Document> const m_pDocument;
  CFX_Font m_Font;
  std::vector<std::unique_ptr<CFX_Font>> m_FontFallbacks;
  RetainPtr<CPDF_StreamAcc> m_pFontFile;
  RetainPtr<CPDF_Dictionary> m_pFontDict;
  ByteString m_BaseFontName;
  mutable std::unique_ptr<CPDF_ToUnicodeMap> m_pToUnicodeMap;
  mutable bool m_bToUnicodeLoaded = false;
  int m_Flags = 0;
  int m_StemV = 0;
  int m_Ascent = 0;
  int m_Descent = 0;
  int m_ItalicAngle = 0;
  FX_RECT m_FontBBox;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_FONT_H_
