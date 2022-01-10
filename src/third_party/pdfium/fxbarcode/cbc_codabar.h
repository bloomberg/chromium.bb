// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXBARCODE_CBC_CODABAR_H_
#define FXBARCODE_CBC_CODABAR_H_

#include "core/fxcrt/widestring.h"
#include "fxbarcode/BC_Library.h"
#include "fxbarcode/cbc_onecode.h"

class CBC_OnedCodaBarWriter;
class CFX_Matrix;

class CBC_Codabar final : public CBC_OneCode {
 public:
  CBC_Codabar();
  ~CBC_Codabar() override;

  // CBC_OneCode:
  BC_TYPE GetType() override;
  bool Encode(WideStringView contents) override;
  bool RenderDevice(CFX_RenderDevice* device,
                    const CFX_Matrix& matrix) override;

 private:
  CBC_OnedCodaBarWriter* GetOnedCodaBarWriter();

  WideString m_renderContents;
};

#endif  // FXBARCODE_CBC_CODABAR_H_
