// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_SYSTEMFONTINFO_IFACE_H_
#define CORE_FXGE_SYSTEMFONTINFO_IFACE_H_

#include <memory>

#include "core/fxge/cfx_fontmapper.h"
#include "third_party/base/span.h"

const uint32_t kTableNAME = FXDWORD_GET_MSBFIRST("name");
const uint32_t kTableTTCF = FXDWORD_GET_MSBFIRST("ttcf");

class SystemFontInfoIface {
 public:
  static std::unique_ptr<SystemFontInfoIface> CreateDefault(
      const char** pUserPaths);

  virtual ~SystemFontInfoIface() = default;

  virtual bool EnumFontList(CFX_FontMapper* pMapper) = 0;
  virtual void* MapFont(int weight,
                        bool bItalic,
                        int charset,
                        int pitch_family,
                        const char* face) = 0;
  virtual void* GetFont(const char* face) = 0;
  virtual uint32_t GetFontData(void* hFont,
                               uint32_t table,
                               pdfium::span<uint8_t> buffer) = 0;
  virtual bool GetFaceName(void* hFont, ByteString* name) = 0;
  virtual bool GetFontCharset(void* hFont, int* charset) = 0;
  virtual int GetFaceIndex(void* hFont);
  virtual void DeleteFont(void* hFont) = 0;
};

#endif  // CORE_FXGE_SYSTEMFONTINFO_IFACE_H_
