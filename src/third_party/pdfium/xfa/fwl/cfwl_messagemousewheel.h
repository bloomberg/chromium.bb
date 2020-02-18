// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_MESSAGEMOUSEWHEEL_H_
#define XFA_FWL_CFWL_MESSAGEMOUSEWHEEL_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"
#include "xfa/fwl/cfwl_message.h"

class CFWL_MessageMouseWheel final : public CFWL_Message {
 public:
  CFWL_MessageMouseWheel(CFWL_Widget* pDstTarget,
                         uint32_t flags,
                         CFX_PointF pos,
                         CFX_PointF delta);
  ~CFWL_MessageMouseWheel() override;

  uint32_t m_dwFlags;
  CFX_PointF m_pos;
  CFX_PointF m_delta;
};

#endif  // XFA_FWL_CFWL_MESSAGEMOUSEWHEEL_H_
