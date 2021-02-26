// Copyright 2020 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pwl/cpwl_cblistbox.h"

#include <utility>

#include "fpdfsdk/pwl/cpwl_combo_box.h"
#include "fpdfsdk/pwl/cpwl_list_ctrl.h"
#include "public/fpdf_fwlevent.h"

CPWL_CBListBox::CPWL_CBListBox(
    const CreateParams& cp,
    std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData)
    : CPWL_ListBox(cp, std::move(pAttachedData)) {}

CPWL_CBListBox::~CPWL_CBListBox() = default;

bool CPWL_CBListBox::OnLButtonUp(uint32_t nFlag, const CFX_PointF& point) {
  CPWL_Wnd::OnLButtonUp(nFlag, point);

  if (!m_bMouseDown)
    return true;

  ReleaseCapture();
  m_bMouseDown = false;

  if (!ClientHitTest(point))
    return true;
  if (CPWL_Wnd* pParent = GetParentWindow())
    pParent->NotifyLButtonUp(this, point);

  return !OnNotifySelectionChanged(false, nFlag);
}

bool CPWL_CBListBox::IsMovementKey(uint16_t nChar) const {
  switch (nChar) {
    case FWL_VKEY_Up:
    case FWL_VKEY_Down:
    case FWL_VKEY_Home:
    case FWL_VKEY_Left:
    case FWL_VKEY_End:
    case FWL_VKEY_Right:
      return true;
    default:
      return false;
  }
}

bool CPWL_CBListBox::OnMovementKeyDown(uint16_t nChar, uint32_t nFlag) {
  ASSERT(IsMovementKey(nChar));

  switch (nChar) {
    case FWL_VKEY_Up:
      m_pListCtrl->OnVK_UP(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Down:
      m_pListCtrl->OnVK_DOWN(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Home:
      m_pListCtrl->OnVK_HOME(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Left:
      m_pListCtrl->OnVK_LEFT(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_End:
      m_pListCtrl->OnVK_END(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
    case FWL_VKEY_Right:
      m_pListCtrl->OnVK_RIGHT(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      break;
  }
  return OnNotifySelectionChanged(true, nFlag);
}

bool CPWL_CBListBox::IsChar(uint16_t nChar, uint32_t nFlag) const {
  return m_pListCtrl->OnChar(nChar, IsSHIFTpressed(nFlag),
                             IsCTRLpressed(nFlag));
}

bool CPWL_CBListBox::OnCharNotify(uint16_t nChar, uint32_t nFlag) {
  if (auto* pComboBox = static_cast<CPWL_ComboBox*>(GetParentWindow()))
    pComboBox->SetSelectText();

  return OnNotifySelectionChanged(true, nFlag);
}
