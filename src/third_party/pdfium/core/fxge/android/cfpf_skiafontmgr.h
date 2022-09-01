// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_
#define CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fxcrt/bytestring.h"
#include "core/fxcrt/fx_codepage_forward.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/cfx_face.h"
#include "core/fxge/freetype/fx_freetype.h"

class CFPF_SkiaFont;
class CFPF_SkiaPathFont;

class CFPF_SkiaFontMgr {
 public:
  CFPF_SkiaFontMgr();
  ~CFPF_SkiaFontMgr();

  void LoadSystemFonts();
  CFPF_SkiaFont* CreateFont(ByteStringView bsFamilyname,
                            FX_Charset uCharset,
                            uint32_t dwStyle);

  bool InitFTLibrary();
  RetainPtr<CFX_Face> GetFontFace(ByteStringView bsFile, int32_t iFaceIndex);

 private:
  void ScanPath(const ByteString& path);
  void ScanFile(const ByteString& file);
  std::unique_ptr<CFPF_SkiaPathFont> ReportFace(RetainPtr<CFX_Face> face,
                                                const ByteString& file);

  bool m_bLoaded = false;
  ScopedFXFTLibraryRec m_FTLibrary;
  std::vector<std::unique_ptr<CFPF_SkiaPathFont>> m_FontFaces;
  std::map<uint32_t, std::unique_ptr<CFPF_SkiaFont>> m_FamilyFonts;
};

#endif  // CORE_FXGE_ANDROID_CFPF_SKIAFONTMGR_H_
