// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_ONED_BC_ONEDIMWRITER_H_
#define FXBARCODE_ONED_BC_ONEDIMWRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/unowned_ptr.h"
#include "core/fxge/cfx_textrenderoptions.h"
#include "fxbarcode/BC_Library.h"
#include "fxbarcode/BC_Writer.h"

class CFX_Font;
class CFX_Matrix;
class CFX_Path;
class CFX_RenderDevice;
class TextCharPos;

class CBC_OneDimWriter : public CBC_Writer {
 public:
  static constexpr CFX_TextRenderOptions GetTextRenderOptions() {
    return CFX_TextRenderOptions(CFX_TextRenderOptions::kLcd);
  }
  static bool HasValidContentSize(WideStringView contents);

  CBC_OneDimWriter();
  ~CBC_OneDimWriter() override;

  virtual bool RenderResult(WideStringView contents,
                            uint8_t* code,
                            int32_t codeLength);
  virtual bool CheckContentValidity(WideStringView contents) = 0;
  virtual WideString FilterContents(WideStringView contents) = 0;
  virtual void SetDataLength(int32_t length);

  void SetPrintChecksum(bool checksum);
  void SetCalcChecksum(bool state);
  void SetFontSize(float size);
  void SetFontStyle(int32_t style);
  void SetFontColor(FX_ARGB color);

  uint8_t* Encode(const ByteString& contents,
                  BC_TYPE format,
                  int32_t& outWidth,
                  int32_t& outHeight);
  bool RenderDeviceResult(CFX_RenderDevice* device,
                          const CFX_Matrix& matrix,
                          WideStringView contents);
  bool SetFont(CFX_Font* cFont);

 protected:
  virtual uint8_t* EncodeWithHint(const ByteString& contents,
                                  BC_TYPE format,
                                  int32_t& outWidth,
                                  int32_t& outHeight,
                                  int32_t hints);
  virtual uint8_t* EncodeImpl(const ByteString& contents,
                              int32_t& outLength) = 0;
  virtual bool ShowChars(WideStringView contents,
                         CFX_RenderDevice* device,
                         const CFX_Matrix& matrix,
                         int32_t barWidth,
                         int32_t multiple);
  void ShowDeviceChars(CFX_RenderDevice* device,
                       const CFX_Matrix& matrix,
                       const ByteString str,
                       float geWidth,
                       TextCharPos* pCharPos,
                       float locX,
                       float locY,
                       int32_t barWidth);
  void CalcTextInfo(const ByteString& text,
                    TextCharPos* charPos,
                    CFX_Font* cFont,
                    float geWidth,
                    int32_t fontSize,
                    float& charsLen);
  int32_t AppendPattern(uint8_t* target,
                        int32_t pos,
                        const int8_t* pattern,
                        int32_t patternLength,
                        bool startColor);

  void RenderVerticalBars(int32_t outputX, int32_t width);

  bool m_bPrintChecksum = true;
  bool m_bCalcChecksum = false;
  bool m_bLeftPadding = false;
  bool m_bRightPadding = false;

  UnownedPtr<CFX_Font> m_pFont;
  float m_fFontSize = 10.0f;
  int32_t m_iFontStyle = 0;
  uint32_t m_fontColor = 0xff000000;
  BC_TEXT_LOC m_locTextLoc = BC_TEXT_LOC::kBelowEmbed;

  int32_t m_iDataLenth = 0;
  size_t m_iContentLen = 0;

  std::vector<CFX_Path> m_output;
  int32_t m_barWidth;
  int32_t m_multiple;
  float m_outputHScale;
};

#endif  // FXBARCODE_ONED_BC_ONEDIMWRITER_H_
