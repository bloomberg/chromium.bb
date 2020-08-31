// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_TOUNICODEMAP_H_
#define CORE_FPDFAPI_FONT_CPDF_TOUNICODEMAP_H_

#include <map>

#include "core/fxcrt/cfx_widetextbuf.h"
#include "core/fxcrt/unowned_ptr.h"

class CPDF_CID2UnicodeMap;
class CPDF_SimpleParser;
class CPDF_Stream;

class CPDF_ToUnicodeMap {
 public:
  explicit CPDF_ToUnicodeMap(const CPDF_Stream* pStream);
  ~CPDF_ToUnicodeMap();

  WideString Lookup(uint32_t charcode) const;
  uint32_t ReverseLookup(wchar_t unicode) const;

 private:
  friend class cpdf_tounicodemap_StringToCode_Test;
  friend class cpdf_tounicodemap_StringToWideString_Test;

  static pdfium::Optional<uint32_t> StringToCode(ByteStringView str);
  static WideString StringToWideString(ByteStringView str);

  void Load(const CPDF_Stream* pStream);
  void HandleBeginBFChar(CPDF_SimpleParser* pParser);
  void HandleBeginBFRange(CPDF_SimpleParser* pParser);
  uint32_t GetUnicode() const;
  void SetCode(uint32_t srccode, WideString destcode);

  std::map<uint32_t, uint32_t> m_Map;
  UnownedPtr<const CPDF_CID2UnicodeMap> m_pBaseMap;
  CFX_WideTextBuf m_MultiCharBuf;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_TOUNICODEMAP_H_
