// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONTMGR_H_
#define CORE_FXGE_CFX_FONTMGR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/observed_ptr.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/fx_freetype.h"
#include "third_party/base/span.h"

class CFX_Face;
class CFX_FontMapper;

class CFX_FontMgr {
 public:
  class FontDesc final : public Retainable, public Observable {
   public:
    CONSTRUCT_VIA_MAKE_RETAIN;
    ~FontDesc() override;

    pdfium::span<uint8_t> FontData() const {
      return {m_pFontData.get(), m_Size};
    }
    void SetFace(size_t index, CFX_Face* face);
    CFX_Face* GetFace(size_t index) const;

   private:
    FontDesc(std::unique_ptr<uint8_t, FxFreeDeleter> pData, size_t size);

    const size_t m_Size;
    std::unique_ptr<uint8_t, FxFreeDeleter> const m_pFontData;
    ObservedPtr<CFX_Face> m_TTCFaces[16];
  };

  // `index` must be less than `CFX_FontMapper::kNumStandardFonts`.
  static pdfium::span<const uint8_t> GetStandardFont(size_t index);
  static pdfium::span<const uint8_t> GetGenericSansFont();
  static pdfium::span<const uint8_t> GetGenericSerifFont();

  CFX_FontMgr();
  ~CFX_FontMgr();

  RetainPtr<FontDesc> GetCachedFontDesc(const ByteString& face_name,
                                        int weight,
                                        bool bItalic);
  RetainPtr<FontDesc> AddCachedFontDesc(
      const ByteString& face_name,
      int weight,
      bool bItalic,
      std::unique_ptr<uint8_t, FxFreeDeleter> pData,
      size_t size);

  RetainPtr<FontDesc> GetCachedTTCFontDesc(size_t ttc_size, uint32_t checksum);
  RetainPtr<FontDesc> AddCachedTTCFontDesc(
      size_t ttc_size,
      uint32_t checksum,
      std::unique_ptr<uint8_t, FxFreeDeleter> pData,
      size_t size);

  RetainPtr<CFX_Face> NewFixedFace(const RetainPtr<FontDesc>& pDesc,
                                   pdfium::span<const uint8_t> span,
                                   size_t face_index);

  // Always present.
  CFX_FontMapper* GetBuiltinMapper() const { return m_pBuiltinMapper.get(); }

  FXFT_LibraryRec* GetFTLibrary() const { return m_FTLibrary.get(); }
  bool FTLibrarySupportsHinting() const { return m_FTLibrarySupportsHinting; }

 private:
  bool FreeTypeVersionSupportsHinting() const;
  bool SetLcdFilterMode() const;

  // Must come before |m_pBuiltinMapper| and |m_FaceMap|.
  ScopedFXFTLibraryRec const m_FTLibrary;
  std::unique_ptr<CFX_FontMapper> m_pBuiltinMapper;
  std::map<ByteString, ObservedPtr<FontDesc>> m_FaceMap;
  const bool m_FTLibrarySupportsHinting;
};

#endif  // CORE_FXGE_CFX_FONTMGR_H_
