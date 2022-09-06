// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2010 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fxbarcode/oned/BC_OnedUPCAWriter.h"

#include <math.h>

#include <algorithm>
#include <vector>

#include "core/fxcrt/fx_extension.h"
#include "core/fxge/cfx_defaultrenderdevice.h"
#include "core/fxge/text_char_pos.h"
#include "fxbarcode/BC_Writer.h"
#include "fxbarcode/oned/BC_OneDimWriter.h"
#include "fxbarcode/oned/BC_OnedEAN13Writer.h"

CBC_OnedUPCAWriter::CBC_OnedUPCAWriter() {
  m_bLeftPadding = true;
  m_bRightPadding = true;
}

CBC_OnedUPCAWriter::~CBC_OnedUPCAWriter() = default;

bool CBC_OnedUPCAWriter::CheckContentValidity(WideStringView contents) {
  return HasValidContentSize(contents) &&
         std::all_of(contents.begin(), contents.end(),
                     [](wchar_t c) { return FXSYS_IsDecimalDigit(c); });
}

WideString CBC_OnedUPCAWriter::FilterContents(WideStringView contents) {
  WideString filtercontents;
  filtercontents.Reserve(contents.GetLength());
  wchar_t ch;
  for (size_t i = 0; i < contents.GetLength(); i++) {
    ch = contents[i];
    if (ch > 175) {
      i++;
      continue;
    }
    if (FXSYS_IsDecimalDigit(ch))
      filtercontents += ch;
  }
  return filtercontents;
}

void CBC_OnedUPCAWriter::InitEANWriter() {
  m_subWriter = std::make_unique<CBC_OnedEAN13Writer>();
}

int32_t CBC_OnedUPCAWriter::CalcChecksum(const ByteString& contents) {
  int32_t odd = 0;
  int32_t even = 0;
  size_t j = 1;
  for (size_t i = contents.GetLength(); i > 0; i--) {
    if (j % 2) {
      odd += FXSYS_DecimalCharToInt(contents[i - 1]);
    } else {
      even += FXSYS_DecimalCharToInt(contents[i - 1]);
    }
    j++;
  }
  int32_t checksum = (odd * 3 + even) % 10;
  checksum = (10 - checksum) % 10;
  return checksum;
}

uint8_t* CBC_OnedUPCAWriter::EncodeWithHint(const ByteString& contents,
                                            BC_TYPE format,
                                            int32_t& outWidth,
                                            int32_t& outHeight,
                                            int32_t hints) {
  if (format != BC_TYPE::kUPCA)
    return nullptr;

  ByteString toEAN13String = '0' + contents;
  m_iDataLenth = 13;
  return m_subWriter->EncodeWithHint(toEAN13String, BC_TYPE::kEAN13, outWidth,
                                     outHeight, hints);
}

uint8_t* CBC_OnedUPCAWriter::EncodeImpl(const ByteString& contents,
                                        int32_t& outLength) {
  return nullptr;
}

bool CBC_OnedUPCAWriter::ShowChars(WideStringView contents,
                                   CFX_RenderDevice* device,
                                   const CFX_Matrix& matrix,
                                   int32_t barWidth,
                                   int32_t multiple) {
  if (!device)
    return false;

  int32_t leftPadding = 7 * multiple;
  int32_t leftPosition = 10 * multiple + leftPadding;
  ByteString str = FX_UTF8Encode(contents);
  size_t length = str.GetLength();
  std::vector<TextCharPos> charpos(length);
  ByteString tempStr = str.Substr(1, 5);
  float strWidth = (float)35 * multiple;
  float blank = 0.0;

  length = tempStr.GetLength();
  int32_t iFontSize = static_cast<int32_t>(fabs(m_fFontSize));
  int32_t iTextHeight = iFontSize + 1;

  CFX_Matrix matr(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect((float)leftPosition, (float)(m_Height - iTextHeight),
                     (float)(leftPosition + strWidth - 0.5), (float)m_Height);
  matr.Concat(matrix);
  FX_RECT re = matr.TransformRect(rect).GetOuterRect();
  device->FillRect(re, kBackgroundColor);
  CFX_Matrix matr1(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect1((float)(leftPosition + 40 * multiple),
                      (float)(m_Height - iTextHeight),
                      (float)((leftPosition + 40 * multiple) + strWidth - 0.5),
                      (float)m_Height);
  matr1.Concat(matrix);
  re = matr1.TransformRect(rect1).GetOuterRect();
  device->FillRect(re, kBackgroundColor);
  float strWidth1 = (float)multiple * 7;
  CFX_Matrix matr2(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect2(0.0, (float)(m_Height - iTextHeight),
                      (float)strWidth1 - 1, (float)m_Height);
  matr2.Concat(matrix);
  re = matr2.TransformRect(rect2).GetOuterRect();
  device->FillRect(re, kBackgroundColor);
  CFX_Matrix matr3(m_outputHScale, 0.0, 0.0, 1.0, 0.0, 0.0);
  CFX_FloatRect rect3((float)(leftPosition + 85 * multiple),
                      (float)(m_Height - iTextHeight),
                      (float)((leftPosition + 85 * multiple) + strWidth1 - 0.5),
                      (float)m_Height);
  matr3.Concat(matrix);
  re = matr3.TransformRect(rect3).GetOuterRect();
  device->FillRect(re, kBackgroundColor);
  strWidth = strWidth * m_outputHScale;

  CalcTextInfo(tempStr, &charpos[1], m_pFont.Get(), strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(1.0, 0.0, 0.0, -1.0,
                              (float)leftPosition * m_outputHScale,
                              (float)(m_Height - iTextHeight + iFontSize));
    affine_matrix1.Concat(matrix);
    device->DrawNormalText(pdfium::make_span(charpos).subspan(1, length),
                           m_pFont.Get(), static_cast<float>(iFontSize),
                           affine_matrix1, m_fontColor, GetTextRenderOptions());
  }
  tempStr = str.Substr(6, 5);
  length = tempStr.GetLength();
  CalcTextInfo(tempStr, &charpos[6], m_pFont.Get(), strWidth, iFontSize, blank);
  {
    CFX_Matrix affine_matrix1(
        1.0, 0.0, 0.0, -1.0,
        (float)(leftPosition + 40 * multiple) * m_outputHScale,
        (float)(m_Height - iTextHeight + iFontSize));
    affine_matrix1.Concat(matrix);
    device->DrawNormalText(pdfium::make_span(charpos).subspan(6, length),
                           m_pFont.Get(), static_cast<float>(iFontSize),
                           affine_matrix1, m_fontColor, GetTextRenderOptions());
  }
  tempStr = str.First(1);
  length = tempStr.GetLength();
  strWidth = (float)multiple * 7;
  strWidth = strWidth * m_outputHScale;

  CalcTextInfo(tempStr, charpos.data(), m_pFont.Get(), strWidth, iFontSize,
               blank);
  {
    CFX_Matrix affine_matrix1(1.0, 0.0, 0.0, -1.0, 0,
                              (float)(m_Height - iTextHeight + iFontSize));
    affine_matrix1.Concat(matrix);
    device->DrawNormalText(pdfium::make_span(charpos).first(length),
                           m_pFont.Get(), static_cast<float>(iFontSize),
                           affine_matrix1, m_fontColor, GetTextRenderOptions());
  }
  tempStr = str.Substr(11, 1);
  length = tempStr.GetLength();
  CalcTextInfo(tempStr, &charpos[11], m_pFont.Get(), strWidth, iFontSize,
               blank);
  {
    CFX_Matrix affine_matrix1(
        1.0, 0.0, 0.0, -1.0,
        (float)(leftPosition + 85 * multiple) * m_outputHScale,
        (float)(m_Height - iTextHeight + iFontSize));
    affine_matrix1.Concat(matrix);
    device->DrawNormalText(pdfium::make_span(charpos).subspan(11, length),
                           m_pFont.Get(), static_cast<float>(iFontSize),
                           affine_matrix1, m_fontColor, GetTextRenderOptions());
  }
  return true;
}
