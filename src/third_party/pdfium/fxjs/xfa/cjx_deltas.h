// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_DELTAS_H_
#define FXJS_XFA_CJX_DELTAS_H_

#include "fxjs/xfa/cjx_list.h"

class CXFA_Deltas;

class CJX_Deltas final : public CJX_List {
 public:
  explicit CJX_Deltas(CXFA_Deltas* node);
  ~CJX_Deltas() override;
};

#endif  // FXJS_XFA_CJX_DELTAS_H_
