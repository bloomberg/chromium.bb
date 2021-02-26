// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_GRAPHSTATEDATA_H_
#define CORE_FXGE_CFX_GRAPHSTATEDATA_H_

#include <vector>

#include "core/fxcrt/fx_system.h"
#include "core/fxcrt/retain_ptr.h"

class CFX_GraphStateData {
 public:
  enum LineCap : uint8_t {
    LineCapButt = 0,
    LineCapRound = 1,
    LineCapSquare = 2
  };

  enum LineJoin : uint8_t {
    LineJoinMiter = 0,
    LineJoinRound = 1,
    LineJoinBevel = 2
  };

  CFX_GraphStateData();
  CFX_GraphStateData(const CFX_GraphStateData& src);
  CFX_GraphStateData(CFX_GraphStateData&& src) noexcept;
  ~CFX_GraphStateData();

  CFX_GraphStateData& operator=(const CFX_GraphStateData& that);
  CFX_GraphStateData& operator=(CFX_GraphStateData&& that) noexcept;

  LineCap m_LineCap = LineCapButt;
  LineJoin m_LineJoin = LineJoinMiter;
  float m_DashPhase = 0.0f;
  float m_MiterLimit = 10.0f;
  float m_LineWidth = 1.0f;
  std::vector<float> m_DashArray;
};

class CFX_RetainableGraphStateData : public Retainable,
                                     public CFX_GraphStateData {
 public:
  CONSTRUCT_VIA_MAKE_RETAIN;

  RetainPtr<CFX_RetainableGraphStateData> Clone() const;

 private:
  CFX_RetainableGraphStateData();
  CFX_RetainableGraphStateData(const CFX_RetainableGraphStateData& src);
  ~CFX_RetainableGraphStateData() override;
};

#endif  // CORE_FXGE_CFX_GRAPHSTATEDATA_H_
