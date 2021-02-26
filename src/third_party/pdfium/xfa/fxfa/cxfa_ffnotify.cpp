// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/cxfa_ffnotify.h"

#include <utility>

#include "third_party/base/check.h"
#include "xfa/fxfa/cxfa_ffapp.h"
#include "xfa/fxfa/cxfa_ffarc.h"
#include "xfa/fxfa/cxfa_ffbarcode.h"
#include "xfa/fxfa/cxfa_ffcheckbutton.h"
#include "xfa/fxfa/cxfa_ffcombobox.h"
#include "xfa/fxfa/cxfa_ffdatetimeedit.h"
#include "xfa/fxfa/cxfa_ffdoc.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffexclgroup.h"
#include "xfa/fxfa/cxfa_fffield.h"
#include "xfa/fxfa/cxfa_ffimage.h"
#include "xfa/fxfa/cxfa_ffimageedit.h"
#include "xfa/fxfa/cxfa_ffline.h"
#include "xfa/fxfa/cxfa_fflistbox.h"
#include "xfa/fxfa/cxfa_ffnumericedit.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffpasswordedit.h"
#include "xfa/fxfa/cxfa_ffpushbutton.h"
#include "xfa/fxfa/cxfa_ffrectangle.h"
#include "xfa/fxfa/cxfa_ffsignature.h"
#include "xfa/fxfa/cxfa_fftext.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/cxfa_ffwidgethandler.h"
#include "xfa/fxfa/cxfa_fwladapterwidgetmgr.h"
#include "xfa/fxfa/cxfa_textlayout.h"
#include "xfa/fxfa/cxfa_textprovider.h"
#include "xfa/fxfa/layout/cxfa_layoutprocessor.h"
#include "xfa/fxfa/parser/cxfa_barcode.h"
#include "xfa/fxfa/parser/cxfa_binditems.h"
#include "xfa/fxfa/parser/cxfa_button.h"
#include "xfa/fxfa/parser/cxfa_checkbutton.h"
#include "xfa/fxfa/parser/cxfa_node.h"
#include "xfa/fxfa/parser/cxfa_passwordedit.h"

CXFA_FFNotify::CXFA_FFNotify(CXFA_FFDoc* pDoc) : m_pDoc(pDoc) {}

CXFA_FFNotify::~CXFA_FFNotify() = default;

void CXFA_FFNotify::Trace(cppgc::Visitor* visitor) const {
  visitor->Trace(m_pDoc);
}

void CXFA_FFNotify::OnPageEvent(CXFA_ViewLayoutItem* pSender,
                                uint32_t dwEvent) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView(pSender->GetLayout());
  if (pDocView)
    pDocView->OnPageEvent(pSender, dwEvent);
}

void CXFA_FFNotify::OnWidgetListItemAdded(CXFA_Node* pSender,
                                          const WideString& wsLabel,
                                          int32_t iIndex) {
  if (pSender->GetFFWidgetType() != XFA_FFWidgetType::kChoiceList)
    return;

  CXFA_FFWidget* pWidget = m_pDoc->GetDocView()->GetWidgetForNode(pSender);
  for (; pWidget; pWidget = pWidget->GetNextFFWidget()) {
    if (pWidget->IsLoaded())
      ToDropDown(ToField(pWidget))->InsertItem(wsLabel, iIndex);
  }
}

void CXFA_FFNotify::OnWidgetListItemRemoved(CXFA_Node* pSender,
                                            int32_t iIndex) {
  if (pSender->GetFFWidgetType() != XFA_FFWidgetType::kChoiceList)
    return;

  CXFA_FFWidget* pWidget = m_pDoc->GetDocView()->GetWidgetForNode(pSender);
  for (; pWidget; pWidget = pWidget->GetNextFFWidget()) {
    if (pWidget->IsLoaded())
      ToDropDown(ToField(pWidget))->DeleteItem(iIndex);
  }
}

CXFA_FFPageView* CXFA_FFNotify::OnCreateViewLayoutItem(CXFA_Node* pNode) {
  if (pNode->GetElementType() != XFA_Element::PageArea)
    return nullptr;

  auto* pLayout = CXFA_LayoutProcessor::FromDocument(m_pDoc->GetXFADoc());
  return cppgc::MakeGarbageCollected<CXFA_FFPageView>(
      m_pDoc->GetHeap()->GetAllocationHandle(), m_pDoc->GetDocView(pLayout),
      pNode);
}

CXFA_FFWidget* CXFA_FFNotify::OnCreateContentLayoutItem(CXFA_Node* pNode) {
  DCHECK(pNode->GetElementType() != XFA_Element::ContentArea);
  DCHECK(pNode->GetElementType() != XFA_Element::PageArea);

  // We only need to create the widget for certain types of objects.
  if (!pNode->HasCreatedUIWidget())
    return nullptr;

  CXFA_FFWidget* pWidget = nullptr;
  switch (pNode->GetFFWidgetType()) {
    case XFA_FFWidgetType::kBarcode: {
      CXFA_Node* child = pNode->GetUIChildNode();
      if (child->GetElementType() != XFA_Element::Barcode)
        return nullptr;

      pWidget = cppgc::MakeGarbageCollected<CXFA_FFBarcode>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode,
          static_cast<CXFA_Barcode*>(child));
      break;
    }
    case XFA_FFWidgetType::kButton: {
      CXFA_Node* child = pNode->GetUIChildNode();
      if (child->GetElementType() != XFA_Element::Button)
        return nullptr;

      pWidget = cppgc::MakeGarbageCollected<CXFA_FFPushButton>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode,
          static_cast<CXFA_Button*>(child));
      break;
    }
    case XFA_FFWidgetType::kCheckButton: {
      CXFA_Node* child = pNode->GetUIChildNode();
      if (child->GetElementType() != XFA_Element::CheckButton)
        return nullptr;

      pWidget = cppgc::MakeGarbageCollected<CXFA_FFCheckButton>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode,
          static_cast<CXFA_CheckButton*>(child));
      break;
    }
    case XFA_FFWidgetType::kChoiceList: {
      if (pNode->IsListBox()) {
        pWidget = cppgc::MakeGarbageCollected<CXFA_FFListBox>(
            m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      } else {
        pWidget = cppgc::MakeGarbageCollected<CXFA_FFComboBox>(
            m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      }
      break;
    }
    case XFA_FFWidgetType::kDateTimeEdit:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFDateTimeEdit>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kImageEdit:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFImageEdit>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kNumericEdit:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFNumericEdit>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kPasswordEdit: {
      CXFA_Node* child = pNode->GetUIChildNode();
      if (child->GetElementType() != XFA_Element::PasswordEdit)
        return nullptr;

      pWidget = cppgc::MakeGarbageCollected<CXFA_FFPasswordEdit>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode,
          static_cast<CXFA_PasswordEdit*>(child));
      break;
    }
    case XFA_FFWidgetType::kSignature:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFSignature>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kTextEdit:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFTextEdit>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kArc:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFArc>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kLine:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFLine>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kRectangle:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFRectangle>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kText:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFText>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kImage:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFImage>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kSubform:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFWidget>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kExclGroup:
      pWidget = cppgc::MakeGarbageCollected<CXFA_FFExclGroup>(
          m_pDoc->GetHeap()->GetAllocationHandle(), pNode);
      break;
    case XFA_FFWidgetType::kNone:
      return nullptr;
  }
  auto* pLayout = CXFA_LayoutProcessor::FromDocument(m_pDoc->GetXFADoc());
  pWidget->SetDocView(m_pDoc->GetDocView(pLayout));
  return pWidget;
}

void CXFA_FFNotify::StartFieldDrawLayout(CXFA_Node* pItem,
                                         float* pCalcWidth,
                                         float* pCalcHeight) {
  pItem->StartWidgetLayout(m_pDoc.Get(), pCalcWidth, pCalcHeight);
}

bool CXFA_FFNotify::RunScript(CXFA_Script* script, CXFA_Node* item) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return false;

  CXFA_EventParam EventParam;
  EventParam.m_eType = XFA_EVENT_Unknown;

  XFA_EventError iRet;
  bool bRet;
  std::tie(iRet, bRet) = item->ExecuteBoolScript(pDocView, script, &EventParam);
  return iRet == XFA_EventError::kSuccess && bRet;
}

XFA_EventError CXFA_FFNotify::ExecEventByDeepFirst(CXFA_Node* pFormNode,
                                                   XFA_EVENTTYPE eEventType,
                                                   bool bIsFormReady,
                                                   bool bRecursive) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return XFA_EventError::kNotExist;
  return pDocView->ExecEventActivityByDeepFirst(pFormNode, eEventType,
                                                bIsFormReady, bRecursive);
}

void CXFA_FFNotify::AddCalcValidate(CXFA_Node* pNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  pDocView->AddCalculateNode(pNode);
  pDocView->AddValidateNode(pNode);
}

IXFA_AppProvider* CXFA_FFNotify::GetAppProvider() {
  return m_pDoc->GetApp()->GetAppProvider();
}

CXFA_FFWidgetHandler* CXFA_FFNotify::GetWidgetHandler() {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  return pDocView ? pDocView->GetWidgetHandler() : nullptr;
}

void CXFA_FFNotify::OpenDropDownList(CXFA_Node* pNode) {
  auto* pDocLayout = CXFA_LayoutProcessor::FromDocument(m_pDoc->GetXFADoc());
  CXFA_LayoutItem* pLayoutItem = pDocLayout->GetLayoutItem(pNode);
  if (!pLayoutItem)
    return;

  CXFA_FFWidget* hWidget = CXFA_FFWidget::FromLayoutItem(pLayoutItem);
  if (!hWidget)
    return;

  CXFA_FFDoc* hDoc = GetFFDoc();
  hDoc->SetFocusWidget(hWidget);
  if (hWidget->GetNode()->GetFFWidgetType() != XFA_FFWidgetType::kChoiceList)
    return;

  if (!hWidget->IsLoaded())
    return;

  CXFA_FFDropDown* pDropDown = ToDropDown(ToField(hWidget));
  CXFA_FFComboBox* pComboBox = ToComboBox(pDropDown);
  if (!pComboBox)
    return;

  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  pDocView->LockUpdate();
  pComboBox->OpenDropDownList();
  pDocView->UnlockUpdate();
  pDocView->UpdateDocView();
}

void CXFA_FFNotify::ResetData(CXFA_Node* pNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  pDocView->ResetNode(pNode);
}

int32_t CXFA_FFNotify::GetLayoutStatus() {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  return pDocView ? pDocView->GetLayoutStatus() : 0;
}

void CXFA_FFNotify::RunNodeInitialize(CXFA_Node* pNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  pDocView->AddNewFormNode(pNode);
}

void CXFA_FFNotify::RunSubformIndexChange(CXFA_Node* pSubformNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  pDocView->AddIndexChangedSubform(pSubformNode);
}

CXFA_Node* CXFA_FFNotify::GetFocusWidgetNode() {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  return pDocView ? pDocView->GetFocusNode() : nullptr;
}

void CXFA_FFNotify::SetFocusWidgetNode(CXFA_Node* pNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;
  pDocView->SetFocusNode(pNode);
}

void CXFA_FFNotify::OnNodeReady(CXFA_Node* pNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  if (pNode->HasCreatedUIWidget()) {
    pNode->SetWidgetReady();
    return;
  }

  switch (pNode->GetElementType()) {
    case XFA_Element::BindItems:
      pDocView->AddBindItem(static_cast<CXFA_BindItems*>(pNode));
      break;
    case XFA_Element::Validate:
      pNode->SetFlag(XFA_NodeFlag_NeedsInitApp);
      break;
    default:
      break;
  }
}

void CXFA_FFNotify::OnValueChanging(CXFA_Node* pSender, XFA_Attribute eAttr) {
  if (eAttr != XFA_Attribute::Presence)
    return;
  if (pSender->GetPacketType() == XFA_PacketType::Datasets)
    return;
  if (!pSender->IsFormContainer())
    return;

  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;
  if (pDocView->GetLayoutStatus() < XFA_DOCVIEW_LAYOUTSTATUS_End)
    return;

  CXFA_FFWidget* pWidget = m_pDoc->GetDocView()->GetWidgetForNode(pSender);
  for (; pWidget; pWidget = pWidget->GetNextFFWidget()) {
    if (pWidget->IsLoaded())
      pWidget->InvalidateRect();
  }
}

void CXFA_FFNotify::OnValueChanged(CXFA_Node* pSender,
                                   XFA_Attribute eAttr,
                                   CXFA_Node* pParentNode,
                                   CXFA_Node* pWidgetNode) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  if (pSender->GetPacketType() != XFA_PacketType::Form) {
    if (eAttr == XFA_Attribute::Value)
      pDocView->AddCalculateNodeNotify(pSender);
    return;
  }

  XFA_Element eType = pParentNode->GetElementType();
  bool bIsContainerNode = pParentNode->IsContainerNode();
  bool bUpdateProperty = false;
  pDocView->SetChangeMark();
  switch (eType) {
    case XFA_Element::Caption: {
      CXFA_TextLayout* pCapOut = pWidgetNode->GetCaptionTextLayout();
      if (!pCapOut)
        return;

      pCapOut->Unload();
      break;
    }
    case XFA_Element::Ui:
    case XFA_Element::Para:
      bUpdateProperty = true;
      break;
    default:
      break;
  }
  if (bIsContainerNode && eAttr == XFA_Attribute::Access)
    bUpdateProperty = true;

  if (eAttr == XFA_Attribute::Value) {
    pDocView->AddCalculateNodeNotify(pSender);
    if (eType == XFA_Element::Value || bIsContainerNode) {
      if (bIsContainerNode) {
        m_pDoc->GetDocView()->UpdateUIDisplay(pWidgetNode, nullptr);
        pDocView->AddCalculateNode(pWidgetNode);
        pDocView->AddValidateNode(pWidgetNode);
      } else if (pWidgetNode->GetParent()->GetElementType() ==
                 XFA_Element::ExclGroup) {
        m_pDoc->GetDocView()->UpdateUIDisplay(pWidgetNode, nullptr);
      }
      return;
    }
  }

  CXFA_FFWidget* pWidget = m_pDoc->GetDocView()->GetWidgetForNode(pWidgetNode);
  for (; pWidget; pWidget = pWidget->GetNextFFWidget()) {
    if (!pWidget->IsLoaded())
      continue;

    if (bUpdateProperty)
      pWidget->UpdateWidgetProperty();
    pWidget->PerformLayout();
    pWidget->InvalidateRect();
  }
}

void CXFA_FFNotify::OnContainerChanged(CXFA_Node* pNode) {
  m_pDoc->GetXFADoc()->GetLayoutProcessor()->AddChangedContainer(pNode);
}

void CXFA_FFNotify::OnChildAdded(CXFA_Node* pSender) {
  if (!pSender->IsFormContainer())
    return;

  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  bool bLayoutReady =
      !(pDocView->m_bInLayoutStatus) &&
      (pDocView->GetLayoutStatus() == XFA_DOCVIEW_LAYOUTSTATUS_End);
  if (bLayoutReady)
    m_pDoc->SetChangeMark();
}

void CXFA_FFNotify::OnChildRemoved() {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView();
  if (!pDocView)
    return;

  bool bLayoutReady =
      !(pDocView->m_bInLayoutStatus) &&
      (pDocView->GetLayoutStatus() == XFA_DOCVIEW_LAYOUTSTATUS_End);
  if (bLayoutReady)
    m_pDoc->SetChangeMark();
}

void CXFA_FFNotify::OnLayoutItemAdded(CXFA_LayoutProcessor* pLayout,
                                      CXFA_LayoutItem* pSender,
                                      int32_t iPageIdx,
                                      uint32_t dwStatus) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView(pLayout);
  if (!pDocView)
    return;

  CXFA_FFWidget* pWidget = CXFA_FFWidget::FromLayoutItem(pSender);
  if (!pWidget)
    return;

  CXFA_FFPageView* pNewPageView = pDocView->GetPageView(iPageIdx);
  uint32_t dwFilter = XFA_WidgetStatus_Visible | XFA_WidgetStatus_Viewable |
                      XFA_WidgetStatus_Printable;
  pWidget->ModifyStatus(dwStatus, dwFilter);
  CXFA_FFPageView* pPrePageView = pWidget->GetPageView();
  if (pPrePageView != pNewPageView ||
      (dwStatus & (XFA_WidgetStatus_Visible | XFA_WidgetStatus_Viewable)) ==
          (XFA_WidgetStatus_Visible | XFA_WidgetStatus_Viewable)) {
    pWidget->SetPageView(pNewPageView);
    m_pDoc->WidgetPostAdd(pWidget);
  }
  if (pDocView->GetLayoutStatus() != XFA_DOCVIEW_LAYOUTSTATUS_End ||
      !(dwStatus & XFA_WidgetStatus_Visible)) {
    return;
  }
  if (pWidget->IsLoaded()) {
    if (pWidget->GetWidgetRect() != pWidget->RecacheWidgetRect())
      pWidget->PerformLayout();
  } else {
    pWidget->LoadWidget();
  }
  pWidget->InvalidateRect();
}

void CXFA_FFNotify::OnLayoutItemRemoving(CXFA_LayoutProcessor* pLayout,
                                         CXFA_LayoutItem* pSender) {
  CXFA_FFDocView* pDocView = m_pDoc->GetDocView(pLayout);
  if (!pDocView)
    return;

  CXFA_FFWidget* pWidget = CXFA_FFWidget::FromLayoutItem(pSender);
  if (!pWidget)
    return;

  pDocView->DeleteLayoutItem(pWidget);
  m_pDoc->WidgetPreRemove(pWidget);
  pWidget->InvalidateRect();
}
