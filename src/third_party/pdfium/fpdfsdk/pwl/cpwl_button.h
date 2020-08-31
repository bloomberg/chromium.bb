// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PWL_CPWL_BUTTON_H_
#define FPDFSDK_PWL_CPWL_BUTTON_H_

#include <memory>

#include "fpdfsdk/pwl/cpwl_wnd.h"
#include "fpdfsdk/pwl/ipwl_systemhandler.h"

class CPWL_Button : public CPWL_Wnd {
 public:
  CPWL_Button(const CreateParams& cp,
              std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData);
  ~CPWL_Button() override;

  // CPWL_Wnd
  bool OnLButtonDown(uint32_t nFlag, const CFX_PointF& point) override;
  bool OnLButtonUp(uint32_t nFlag, const CFX_PointF& point) override;

 protected:
  bool m_bMouseDown = false;
};

#endif  // FPDFSDK_PWL_CPWL_BUTTON_H_
