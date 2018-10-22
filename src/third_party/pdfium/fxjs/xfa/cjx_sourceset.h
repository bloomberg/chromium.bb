// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FXJS_XFA_CJX_SOURCESET_H_
#define FXJS_XFA_CJX_SOURCESET_H_

#include "fxjs/jse_define.h"
#include "fxjs/xfa/cjx_model.h"

class CXFA_SourceSet;

class CJX_SourceSet final : public CJX_Model {
 public:
  explicit CJX_SourceSet(CXFA_SourceSet* node);
  ~CJX_SourceSet() override;

  JSE_PROP(use);
  JSE_PROP(usehref);
};

#endif  // FXJS_XFA_CJX_SOURCESET_H_
