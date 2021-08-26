// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_combolist.h"

#include "third_party/base/check.h"
#include "xfa/fwl/cfwl_combobox.h"
#include "xfa/fwl/cfwl_comboedit.h"
#include "xfa/fwl/cfwl_listbox.h"
#include "xfa/fwl/cfwl_messagekey.h"
#include "xfa/fwl/cfwl_messagekillfocus.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/fwl_widgetdef.h"

CFWL_ComboList::CFWL_ComboList(CFWL_App* app,
                               const Properties& properties,
                               CFWL_Widget* pOuter)
    : CFWL_ListBox(app, properties, pOuter) {
  DCHECK(pOuter);
}

CFWL_ComboList::~CFWL_ComboList() = default;

int32_t CFWL_ComboList::MatchItem(WideStringView wsMatch) {
  if (wsMatch.IsEmpty())
    return -1;

  int32_t iCount = CountItems(this);
  for (int32_t i = 0; i < iCount; i++) {
    CFWL_ListBox::Item* hItem = GetItem(this, i);
    WideString wsText = hItem ? hItem->GetText() : WideString();
    auto pos = wsText.Find(wsMatch);
    if (pos.has_value() && pos.value() == 0)
      return i;
  }
  return -1;
}

void CFWL_ComboList::ChangeSelected(int32_t iSel) {
  CFWL_ListBox::Item* hItem = GetItem(this, iSel);
  CFWL_ListBox::Item* hOld = GetSelItem(0);
  int32_t iOld = GetItemIndex(this, hOld);
  if (iOld == iSel)
    return;

  CFX_RectF rtInvalidate;
  if (iOld > -1) {
    if (CFWL_ListBox::Item* hOldItem = GetItem(this, iOld))
      rtInvalidate = hOldItem->GetRect();
    SetSelItem(hOld, false);
  }
  if (hItem) {
    if (CFWL_ListBox::Item* hOldItem = GetItem(this, iSel))
      rtInvalidate.Union(hOldItem->GetRect());
    CFWL_ListBox::Item* hSel = GetItem(this, iSel);
    SetSelItem(hSel, true);
  }
  if (!rtInvalidate.IsEmpty())
    RepaintRect(rtInvalidate);
}

CFX_PointF CFWL_ComboList::ClientToOuter(const CFX_PointF& point) {
  return point + CFX_PointF(m_WidgetRect.left, m_WidgetRect.top);
}

void CFWL_ComboList::OnProcessMessage(CFWL_Message* pMessage) {
  CFWL_Message::Type type = pMessage->GetType();
  bool backDefault = true;
  if (type == CFWL_Message::Type::kSetFocus ||
      type == CFWL_Message::Type::kKillFocus) {
    OnDropListFocusChanged(pMessage, type == CFWL_Message::Type::kSetFocus);
  } else if (type == CFWL_Message::Type::kMouse) {
    CFWL_MessageMouse* pMsg = static_cast<CFWL_MessageMouse*>(pMessage);
    CFWL_ScrollBar* vertSB = GetVertScrollBar();
    if (IsShowVertScrollBar() && vertSB) {
      CFX_RectF rect = vertSB->GetWidgetRect();
      if (rect.Contains(pMsg->m_pos)) {
        pMsg->m_pos -= rect.TopLeft();
        vertSB->GetDelegate()->OnProcessMessage(pMsg);
        return;
      }
    }
    switch (pMsg->m_dwCmd) {
      case CFWL_MessageMouse::MouseCommand::kMove:
        backDefault = false;
        OnDropListMouseMove(pMsg);
        break;
      case CFWL_MessageMouse::MouseCommand::kLeftButtonDown:
        backDefault = false;
        OnDropListLButtonDown(pMsg);
        break;
      case CFWL_MessageMouse::MouseCommand::kLeftButtonUp:
        backDefault = false;
        OnDropListLButtonUp(pMsg);
        break;
      default:
        break;
    }
  } else if (type == CFWL_Message::Type::kKey) {
    backDefault = !OnDropListKey(static_cast<CFWL_MessageKey*>(pMessage));
  }
  if (backDefault)
    CFWL_ListBox::OnProcessMessage(pMessage);
}

void CFWL_ComboList::OnDropListFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (bSet)
    return;

  CFWL_MessageKillFocus* pKill = static_cast<CFWL_MessageKillFocus*>(pMsg);
  CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
  if (pKill->IsFocusedOnWidget(pOuter) ||
      pKill->IsFocusedOnWidget(pOuter->GetComboEdit())) {
    pOuter->HideDropDownList();
  }
}

void CFWL_ComboList::OnDropListMouseMove(CFWL_MessageMouse* pMsg) {
  if (GetRTClient().Contains(pMsg->m_pos)) {
    if (m_bNotifyOwner)
      m_bNotifyOwner = false;

    CFWL_ScrollBar* vertSB = GetVertScrollBar();
    if (IsShowVertScrollBar() && vertSB) {
      CFX_RectF rect = vertSB->GetWidgetRect();
      if (rect.Contains(pMsg->m_pos))
        return;
    }

    CFWL_ListBox::Item* hItem = GetItemAtPoint(pMsg->m_pos);
    if (!hItem)
      return;

    ChangeSelected(GetItemIndex(this, hItem));
  } else if (m_bNotifyOwner) {
    pMsg->m_pos = ClientToOuter(pMsg->m_pos);

    CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
    pOuter->GetDelegate()->OnProcessMessage(pMsg);
  }
}

void CFWL_ComboList::OnDropListLButtonDown(CFWL_MessageMouse* pMsg) {
  if (GetRTClient().Contains(pMsg->m_pos))
    return;

  CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
  pOuter->HideDropDownList();
}

void CFWL_ComboList::OnDropListLButtonUp(CFWL_MessageMouse* pMsg) {
  CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
  if (m_bNotifyOwner) {
    pMsg->m_pos = ClientToOuter(pMsg->m_pos);
    pOuter->GetDelegate()->OnProcessMessage(pMsg);
    return;
  }

  CFWL_ScrollBar* vertSB = GetVertScrollBar();
  if (IsShowVertScrollBar() && vertSB) {
    CFX_RectF rect = vertSB->GetWidgetRect();
    if (rect.Contains(pMsg->m_pos))
      return;
  }
  pOuter->HideDropDownList();

  CFWL_ListBox::Item* hItem = GetItemAtPoint(pMsg->m_pos);
  if (hItem)
    pOuter->ProcessSelChanged(true);
}

bool CFWL_ComboList::OnDropListKey(CFWL_MessageKey* pKey) {
  CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
  bool bPropagate = false;
  if (pKey->m_dwCmd == CFWL_MessageKey::KeyCommand::kKeyDown) {
    uint32_t dwKeyCode = pKey->m_dwKeyCodeOrChar;
    switch (dwKeyCode) {
      case XFA_FWL_VKEY_Return:
      case XFA_FWL_VKEY_Escape: {
        pOuter->HideDropDownList();
        return true;
      }
      case XFA_FWL_VKEY_Up:
      case XFA_FWL_VKEY_Down: {
        OnDropListKeyDown(pKey);
        pOuter->ProcessSelChanged(false);
        return true;
      }
      default: {
        bPropagate = true;
        break;
      }
    }
  } else if (pKey->m_dwCmd == CFWL_MessageKey::KeyCommand::kChar) {
    bPropagate = true;
  }
  if (bPropagate) {
    pKey->SetDstTarget(GetOuter());
    pOuter->GetDelegate()->OnProcessMessage(pKey);
    return true;
  }
  return false;
}

void CFWL_ComboList::OnDropListKeyDown(CFWL_MessageKey* pKey) {
  auto dwKeyCode = static_cast<XFA_FWL_VKEYCODE>(pKey->m_dwKeyCodeOrChar);
  switch (dwKeyCode) {
    case XFA_FWL_VKEY_Up:
    case XFA_FWL_VKEY_Down:
    case XFA_FWL_VKEY_Home:
    case XFA_FWL_VKEY_End: {
      CFWL_ComboBox* pOuter = static_cast<CFWL_ComboBox*>(GetOuter());
      CFWL_ListBox::Item* hItem = GetItem(this, pOuter->GetCurrentSelection());
      hItem = GetListItem(hItem, dwKeyCode);
      if (!hItem)
        break;

      SetSelection(hItem, hItem, true);
      ScrollToVisible(hItem);
      RepaintRect(CFX_RectF(0, 0, m_WidgetRect.width, m_WidgetRect.height));
      break;
    }
    default:
      break;
  }
}
