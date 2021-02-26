// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_CSS_CFX_CSSVALUELIST_H_
#define CORE_FXCRT_CSS_CFX_CSSVALUELIST_H_

#include <vector>

#include "core/fxcrt/css/cfx_cssvalue.h"

class CFX_CSSValueList final : public CFX_CSSValue {
 public:
  explicit CFX_CSSValueList(std::vector<RetainPtr<CFX_CSSValue>> list);
  ~CFX_CSSValueList() override;

  const std::vector<RetainPtr<CFX_CSSValue>>& values() const { return list_; }

 private:
  std::vector<RetainPtr<CFX_CSSValue>> list_;
};

#endif  // CORE_FXCRT_CSS_CFX_CSSVALUELIST_H_
