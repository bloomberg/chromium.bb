// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_FONTMAPPER_H_
#define CORE_FXGE_CFX_FONTMAPPER_H_

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_codepage_forward.h"
#include "core/fxcrt/fx_memory_wrappers.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_face.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class CFX_FontMgr;
class CFX_SubstFont;
class SystemFontInfoIface;

class CFX_FontMapper {
 public:
  enum StandardFont : uint8_t {
    kCourier = 0,
    kCourierBold,
    kCourierBoldOblique,
    kCourierOblique,
    kHelvetica,
    kHelveticaBold,
    kHelveticaBoldOblique,
    kHelveticaOblique,
    kTimes,
    kTimesBold,
    kTimesBoldOblique,
    kTimesOblique,
    kSymbol,
    kDingbats,
    kLast = kDingbats
  };
  static constexpr int kNumStandardFonts = 14;

  explicit CFX_FontMapper(CFX_FontMgr* mgr);
  ~CFX_FontMapper();

  static absl::optional<StandardFont> GetStandardFontName(ByteString* name);
  static bool IsStandardFontName(const ByteString& name);
  static bool IsSymbolicFont(StandardFont font);
  static bool IsFixedFont(StandardFont font);
  static constexpr uint32_t MakeTag(char c1, char c2, char c3, char c4) {
    return static_cast<uint8_t>(c1) << 24 | static_cast<uint8_t>(c2) << 16 |
           static_cast<uint8_t>(c3) << 8 | static_cast<uint8_t>(c4);
  }

  void SetSystemFontInfo(std::unique_ptr<SystemFontInfoIface> pFontInfo);
  std::unique_ptr<SystemFontInfoIface> TakeSystemFontInfo();
  void AddInstalledFont(const ByteString& name, FX_Charset charset);
  void LoadInstalledFonts();

  RetainPtr<CFX_Face> FindSubstFont(const ByteString& face_name,
                                    bool bTrueType,
                                    uint32_t flags,
                                    int weight,
                                    int italic_angle,
                                    FX_CodePage code_page,
                                    CFX_SubstFont* pSubstFont);

  size_t GetFaceSize() const;
  ByteString GetFaceName(size_t index) const { return m_FaceArray[index].name; }
  bool HasInstalledFont(ByteStringView name) const;
  bool HasLocalizedFont(ByteStringView name) const;

#if BUILDFLAG(IS_WIN)
  absl::optional<ByteString> InstalledFontNameStartingWith(
      const ByteString& name) const;
  absl::optional<ByteString> LocalizedFontNameStartingWith(
      const ByteString& name) const;
#endif  // BUILDFLAG(IS_WIN)

#ifdef PDF_ENABLE_XFA
  std::unique_ptr<uint8_t, FxFreeDeleter> RawBytesForIndex(
      size_t index,
      size_t* returned_length);
#endif  // PDF_ENABLE_XFA

 private:
  uint32_t GetChecksumFromTT(void* hFont);
  ByteString GetPSNameFromTT(void* hFont);
  ByteString MatchInstalledFonts(const ByteString& norm_name);
  RetainPtr<CFX_Face> UseInternalSubst(CFX_SubstFont* pSubstFont,
                                       int iBaseFont,
                                       int italic_angle,
                                       int weight,
                                       int pitch_family);
  RetainPtr<CFX_Face> GetCachedTTCFace(void* hFont,
                                       uint32_t ttc_size,
                                       uint32_t font_size);
  RetainPtr<CFX_Face> GetCachedFace(void* hFont,
                                    ByteString SubstName,
                                    int weight,
                                    bool bItalic,
                                    uint32_t font_size);

  struct FaceData {
    ByteString name;
    uint32_t charset;
  };

  bool m_bListLoaded = false;
  ByteString m_LastFamily;
  std::vector<FaceData> m_FaceArray;
  std::unique_ptr<SystemFontInfoIface> m_pFontInfo;
  UnownedPtr<CFX_FontMgr> const m_pFontMgr;
  std::vector<ByteString> m_InstalledTTFonts;
  std::vector<std::pair<ByteString, ByteString>> m_LocalizedTTFonts;
  RetainPtr<CFX_Face> m_StandardFaces[kNumStandardFonts];
  RetainPtr<CFX_Face> m_GenericSansFace;
  RetainPtr<CFX_Face> m_GenericSerifFace;
};

#endif  // CORE_FXGE_CFX_FONTMAPPER_H_
