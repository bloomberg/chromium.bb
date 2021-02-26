// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/pwl/cpwl_combo_box.h"

#include <algorithm>
#include <utility>

#include "fpdfsdk/pwl/cpwl_cbbutton.h"
#include "fpdfsdk/pwl/cpwl_cblistbox.h"
#include "fpdfsdk/pwl/cpwl_edit.h"
#include "fpdfsdk/pwl/ipwl_fillernotify.h"
#include "public/fpdf_fwlevent.h"

namespace {

constexpr float kComboBoxDefaultFontSize = 12.0f;
constexpr int kDefaultButtonWidth = 13;

}  // namespace

CPWL_ComboBox::CPWL_ComboBox(
    const CreateParams& cp,
    std::unique_ptr<IPWL_SystemHandler::PerWindowData> pAttachedData)
    : CPWL_Wnd(cp, std::move(pAttachedData)) {
  GetCreationParams()->dwFlags &= ~PWS_HSCROLL;
  GetCreationParams()->dwFlags &= ~PWS_VSCROLL;
}

CPWL_ComboBox::~CPWL_ComboBox() = default;

void CPWL_ComboBox::OnDestroy() {
  // Until cleanup takes place in the virtual destructor for CPWL_Wnd
  // subclasses, implement the virtual OnDestroy method that does the
  // cleanup first, then invokes the superclass OnDestroy ... gee,
  // like a dtor would.
  m_pList.Release();
  m_pButton.Release();
  m_pEdit.Release();
  CPWL_Wnd::OnDestroy();
}

void CPWL_ComboBox::SetFocus() {
  if (m_pEdit)
    m_pEdit->SetFocus();
}

void CPWL_ComboBox::KillFocus() {
  if (!SetPopup(false))
    return;

  CPWL_Wnd::KillFocus();
}

WideString CPWL_ComboBox::GetSelectedText() {
  if (m_pEdit)
    return m_pEdit->GetSelectedText();

  return WideString();
}

void CPWL_ComboBox::ReplaceSelection(const WideString& text) {
  if (m_pEdit)
    m_pEdit->ReplaceSelection(text);
}

bool CPWL_ComboBox::SelectAllText() {
  return m_pEdit && m_pEdit->SelectAllText();
}

bool CPWL_ComboBox::CanUndo() {
  return m_pEdit && m_pEdit->CanUndo();
}

bool CPWL_ComboBox::CanRedo() {
  return m_pEdit && m_pEdit->CanRedo();
}

bool CPWL_ComboBox::Undo() {
  return m_pEdit && m_pEdit->Undo();
}

bool CPWL_ComboBox::Redo() {
  return m_pEdit && m_pEdit->Redo();
}

WideString CPWL_ComboBox::GetText() {
  return m_pEdit ? m_pEdit->GetText() : WideString();
}

void CPWL_ComboBox::SetText(const WideString& text) {
  if (m_pEdit)
    m_pEdit->SetText(text);
}

void CPWL_ComboBox::AddString(const WideString& str) {
  if (m_pList)
    m_pList->AddString(str);
}

int32_t CPWL_ComboBox::GetSelect() const {
  return m_nSelectItem;
}

void CPWL_ComboBox::SetSelect(int32_t nItemIndex) {
  if (m_pList)
    m_pList->Select(nItemIndex);

  m_pEdit->SetText(m_pList->GetText());
  m_nSelectItem = nItemIndex;
}

void CPWL_ComboBox::SetEditSelection(int32_t nStartChar, int32_t nEndChar) {
  if (m_pEdit)
    m_pEdit->SetSelection(nStartChar, nEndChar);
}

std::pair<int32_t, int32_t> CPWL_ComboBox::GetEditSelection() const {
  if (!m_pEdit)
    return std::make_pair(-1, -1);
  return m_pEdit->GetSelection();
}

void CPWL_ComboBox::ClearSelection() {
  if (m_pEdit)
    m_pEdit->ClearSelection();
}

void CPWL_ComboBox::CreateChildWnd(const CreateParams& cp) {
  CreateEdit(cp);
  CreateButton(cp);
  CreateListBox(cp);
}

void CPWL_ComboBox::CreateEdit(const CreateParams& cp) {
  if (m_pEdit)
    return;

  CreateParams ecp = cp;
  ecp.dwFlags = PWS_VISIBLE | PWS_CHILD | PWS_BORDER | PES_CENTER |
                PES_AUTOSCROLL | PES_UNDO;

  if (HasFlag(PWS_AUTOFONTSIZE))
    ecp.dwFlags |= PWS_AUTOFONTSIZE;

  if (!HasFlag(PCBS_ALLOWCUSTOMTEXT))
    ecp.dwFlags |= PWS_READONLY;

  ecp.rcRectWnd = CFX_FloatRect();
  ecp.dwBorderWidth = 0;
  ecp.nBorderStyle = BorderStyle::kSolid;

  auto pEdit = std::make_unique<CPWL_Edit>(ecp, CloneAttachedData());
  m_pEdit = pEdit.get();
  m_pEdit->AttachFFLData(m_pFormFiller.Get());
  AddChild(std::move(pEdit));
  m_pEdit->Realize();
}

void CPWL_ComboBox::CreateButton(const CreateParams& cp) {
  if (m_pButton)
    return;

  CreateParams bcp = cp;
  bcp.dwFlags = PWS_VISIBLE | PWS_CHILD | PWS_BORDER | PWS_BACKGROUND;
  bcp.sBackgroundColor = CFX_Color(CFX_Color::kRGB, 220.0f / 255.0f,
                                   220.0f / 255.0f, 220.0f / 255.0f);
  bcp.sBorderColor = PWL_DEFAULT_BLACKCOLOR;
  bcp.dwBorderWidth = 2;
  bcp.nBorderStyle = BorderStyle::kBeveled;
  bcp.eCursorType = FXCT_ARROW;

  auto pButton = std::make_unique<CPWL_CBButton>(bcp, CloneAttachedData());
  m_pButton = pButton.get();
  AddChild(std::move(pButton));
  m_pButton->Realize();
}

void CPWL_ComboBox::CreateListBox(const CreateParams& cp) {
  if (m_pList)
    return;

  CreateParams lcp = cp;
  lcp.dwFlags =
      PWS_CHILD | PWS_BORDER | PWS_BACKGROUND | PLBS_HOVERSEL | PWS_VSCROLL;
  lcp.nBorderStyle = BorderStyle::kSolid;
  lcp.dwBorderWidth = 1;
  lcp.eCursorType = FXCT_ARROW;
  lcp.rcRectWnd = CFX_FloatRect();

  lcp.fFontSize =
      (cp.dwFlags & PWS_AUTOFONTSIZE) ? kComboBoxDefaultFontSize : cp.fFontSize;

  if (cp.sBorderColor.nColorType == CFX_Color::kTransparent)
    lcp.sBorderColor = PWL_DEFAULT_BLACKCOLOR;

  if (cp.sBackgroundColor.nColorType == CFX_Color::kTransparent)
    lcp.sBackgroundColor = PWL_DEFAULT_WHITECOLOR;

  auto pList = std::make_unique<CPWL_CBListBox>(lcp, CloneAttachedData());
  m_pList = pList.get();
  m_pList->AttachFFLData(m_pFormFiller.Get());
  AddChild(std::move(pList));
  m_pList->Realize();
}

bool CPWL_ComboBox::RePosChildWnd() {
  ObservedPtr<CPWL_ComboBox> thisObserved(this);
  const CFX_FloatRect rcClient = GetClientRect();
  if (m_bPopup) {
    const float fOldWindowHeight = m_rcOldWindow.Height();
    const float fOldClientHeight = fOldWindowHeight - GetBorderWidth() * 2;

    CFX_FloatRect rcList = CPWL_Wnd::GetWindowRect();
    CFX_FloatRect rcButton = rcClient;
    rcButton.left =
        std::max(rcButton.right - kDefaultButtonWidth, rcClient.left);
    CFX_FloatRect rcEdit = rcClient;
    rcEdit.right = std::max(rcButton.left - 1.0f, rcEdit.left);
    if (m_bBottom) {
      rcButton.bottom = rcButton.top - fOldClientHeight;
      rcEdit.bottom = rcEdit.top - fOldClientHeight;
      rcList.top -= fOldWindowHeight;
    } else {
      rcButton.top = rcButton.bottom + fOldClientHeight;
      rcEdit.top = rcEdit.bottom + fOldClientHeight;
      rcList.bottom += fOldWindowHeight;
    }

    if (m_pButton) {
      m_pButton->Move(rcButton, true, false);
      if (!thisObserved)
        return false;
    }

    if (m_pEdit) {
      m_pEdit->Move(rcEdit, true, false);
      if (!thisObserved)
        return false;
    }

    if (m_pList) {
      if (!m_pList->SetVisible(true) || !thisObserved)
        return false;

      if (!m_pList->Move(rcList, true, false) || !thisObserved)
        return false;

      m_pList->ScrollToListItem(m_nSelectItem);
      if (!thisObserved)
        return false;
    }
    return true;
  }

  CFX_FloatRect rcButton = rcClient;
  rcButton.left = std::max(rcButton.right - kDefaultButtonWidth, rcClient.left);

  if (m_pButton) {
    m_pButton->Move(rcButton, true, false);
    if (!thisObserved)
      return false;
  }

  CFX_FloatRect rcEdit = rcClient;
  rcEdit.right = std::max(rcButton.left - 1.0f, rcEdit.left);

  if (m_pEdit) {
    m_pEdit->Move(rcEdit, true, false);
    if (!thisObserved)
      return false;
  }

  if (m_pList) {
    m_pList->SetVisible(false);
    if (!thisObserved)
      return false;
  }

  return true;
}

void CPWL_ComboBox::SelectAll() {
  if (m_pEdit && HasFlag(PCBS_ALLOWCUSTOMTEXT))
    m_pEdit->SelectAllText();
}

CFX_FloatRect CPWL_ComboBox::GetFocusRect() const {
  return CFX_FloatRect();
}

bool CPWL_ComboBox::SetPopup(bool bPopup) {
  if (!m_pList)
    return true;
  if (bPopup == m_bPopup)
    return true;
  float fListHeight = m_pList->GetContentRect().Height();
  if (!IsFloatBigger(fListHeight, 0.0f))
    return true;

  if (!bPopup) {
    m_bPopup = bPopup;
    return Move(m_rcOldWindow, true, true);
  }

  if (!m_pFillerNotify)
    return true;

  ObservedPtr<CPWL_ComboBox> thisObserved(this);
  if (m_pFillerNotify->OnPopupPreOpen(GetAttachedData(), 0))
    return !!thisObserved;
  if (!thisObserved)
    return false;

  float fBorderWidth = m_pList->GetBorderWidth() * 2;
  float fPopupMin = 0.0f;
  if (m_pList->GetCount() > 3)
    fPopupMin = m_pList->GetFirstHeight() * 3 + fBorderWidth;
  float fPopupMax = fListHeight + fBorderWidth;

  bool bBottom;
  float fPopupRet;
  m_pFillerNotify->QueryWherePopup(GetAttachedData(), fPopupMin, fPopupMax,
                                   &bBottom, &fPopupRet);
  if (!IsFloatBigger(fPopupRet, 0.0f))
    return true;

  m_rcOldWindow = CPWL_Wnd::GetWindowRect();
  m_bPopup = bPopup;
  m_bBottom = bBottom;

  CFX_FloatRect rcWindow = m_rcOldWindow;
  if (bBottom)
    rcWindow.bottom -= fPopupRet;
  else
    rcWindow.top += fPopupRet;

  if (!Move(rcWindow, true, true))
    return false;

  m_pFillerNotify->OnPopupPostOpen(GetAttachedData(), 0);
  return !!thisObserved;
}

bool CPWL_ComboBox::OnKeyDown(uint16_t nChar, uint32_t nFlag) {
  if (!m_pList)
    return false;
  if (!m_pEdit)
    return false;

  m_nSelectItem = -1;

  switch (nChar) {
    case FWL_VKEY_Up:
      if (m_pList->GetCurSel() > 0) {
        if (m_pFillerNotify) {
          if (m_pFillerNotify->OnPopupPreOpen(GetAttachedData(), nFlag))
            return false;
          if (m_pFillerNotify->OnPopupPostOpen(GetAttachedData(), nFlag))
            return false;
        }
        if (m_pList->IsMovementKey(nChar)) {
          if (m_pList->OnMovementKeyDown(nChar, nFlag))
            return false;
          SetSelectText();
        }
      }
      return true;
    case FWL_VKEY_Down:
      if (m_pList->GetCurSel() < m_pList->GetCount() - 1) {
        if (m_pFillerNotify) {
          if (m_pFillerNotify->OnPopupPreOpen(GetAttachedData(), nFlag))
            return false;
          if (m_pFillerNotify->OnPopupPostOpen(GetAttachedData(), nFlag))
            return false;
        }
        if (m_pList->IsMovementKey(nChar)) {
          if (m_pList->OnMovementKeyDown(nChar, nFlag))
            return false;
          SetSelectText();
        }
      }
      return true;
  }

  if (HasFlag(PCBS_ALLOWCUSTOMTEXT))
    return m_pEdit->OnKeyDown(nChar, nFlag);

  return false;
}

bool CPWL_ComboBox::OnChar(uint16_t nChar, uint32_t nFlag) {
  if (!m_pList)
    return false;

  if (!m_pEdit)
    return false;

  // In a combo box if the ENTER/SPACE key is pressed, show the combo box
  // options.
  switch (nChar) {
    case FWL_VKEY_Return:
      SetPopup(!IsPopup());
      SetSelectText();
      return true;
    case FWL_VKEY_Space:
      // Show the combo box options with space only if the combo box is not
      // editable
      if (!HasFlag(PCBS_ALLOWCUSTOMTEXT)) {
        if (!IsPopup()) {
          SetPopup(/*bPopUp=*/true);
          SetSelectText();
        }
        return true;
      }
      break;
    default:
      break;
  }

  m_nSelectItem = -1;
  if (HasFlag(PCBS_ALLOWCUSTOMTEXT))
    return m_pEdit->OnChar(nChar, nFlag);

  if (m_pFillerNotify) {
    if (m_pFillerNotify->OnPopupPreOpen(GetAttachedData(), nFlag))
      return false;
    if (m_pFillerNotify->OnPopupPostOpen(GetAttachedData(), nFlag))
      return false;
  }
  if (!m_pList->IsChar(nChar, nFlag))
    return false;
  return m_pList->OnCharNotify(nChar, nFlag);
}

void CPWL_ComboBox::NotifyLButtonDown(CPWL_Wnd* child, const CFX_PointF& pos) {
  if (child == m_pButton) {
    SetPopup(!m_bPopup);
    // Note, |this| may no longer be viable at this point. If more work needs to
    // be done, check the return value of SetPopup().
  }
}

void CPWL_ComboBox::NotifyLButtonUp(CPWL_Wnd* child, const CFX_PointF& pos) {
  if (!m_pEdit || !m_pList || child != m_pList)
    return;

  SetSelectText();
  SelectAllText();
  m_pEdit->SetFocus();
  SetPopup(false);
  // Note, |this| may no longer be viable at this point. If more work needs to
  // be done, check the return value of SetPopup().
}

bool CPWL_ComboBox::IsPopup() const {
  return m_bPopup;
}

void CPWL_ComboBox::SetSelectText() {
  m_pEdit->SelectAllText();
  m_pEdit->ReplaceSelection(m_pList->GetText());
  m_pEdit->SelectAllText();
  m_nSelectItem = m_pList->GetCurSel();
}

void CPWL_ComboBox::SetFillerNotify(IPWL_FillerNotify* pNotify) {
  m_pFillerNotify = pNotify;

  if (m_pEdit)
    m_pEdit->SetFillerNotify(pNotify);

  if (m_pList)
    m_pList->SetFillerNotify(pNotify);
}
