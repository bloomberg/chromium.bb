// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_CBC_CODEBASE_H_
#define FXBARCODE_CBC_CODEBASE_H_

#include <stdint.h>

#include <memory>

#include "core/fxcrt/widestring.h"
#include "core/fxge/dib/fx_dib.h"
#include "fxbarcode/BC_Library.h"

class CBC_Writer;
class CFX_Matrix;
class CFX_RenderDevice;

class CBC_CodeBase {
 public:
  explicit CBC_CodeBase(std::unique_ptr<CBC_Writer> pWriter);
  virtual ~CBC_CodeBase();

  virtual BC_TYPE GetType() = 0;
  virtual bool Encode(WideStringView contents) = 0;
  virtual bool RenderDevice(CFX_RenderDevice* device,
                            const CFX_Matrix& matrix) = 0;

  void SetTextLocation(BC_TEXT_LOC location);
  bool SetWideNarrowRatio(int8_t ratio);
  bool SetStartChar(char start);
  bool SetEndChar(char end);
  bool SetErrorCorrectionLevel(int32_t level);
  void SetCharEncoding(BC_CHAR_ENCODING encoding);
  bool SetModuleHeight(int32_t moduleHeight);
  bool SetModuleWidth(int32_t moduleWidth);
  void SetHeight(int32_t height);
  void SetWidth(int32_t width);

 protected:
  std::unique_ptr<CBC_Writer> m_pBCWriter;
};

#endif  // FXBARCODE_CBC_CODEBASE_H_
