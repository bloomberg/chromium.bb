// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/layout/cxfa_layoutitem.h"

#include "fxjs/xfa/cjx_object.h"
#include "xfa/fxfa/cxfa_ffnotify.h"
#include "xfa/fxfa/layout/cxfa_contentlayoutitem.h"
#include "xfa/fxfa/layout/cxfa_viewlayoutitem.h"
#include "xfa/fxfa/parser/cxfa_margin.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/cxfa_node.h"

void XFA_ReleaseLayoutItem(CXFA_LayoutItem* pLayoutItem) {
  CXFA_LayoutItem* pNode = pLayoutItem->GetFirstChild();
  CXFA_Document* pDocument = pLayoutItem->GetFormNode()->GetDocument();
  CXFA_FFNotify* pNotify = pDocument->GetNotify();
  CXFA_LayoutProcessor* pDocLayout = pDocument->GetLayoutProcessor();
  while (pNode) {
    CXFA_LayoutItem* pNext = pNode->GetNextSibling();
    pNode->SetParent(nullptr);
    pNotify->OnLayoutItemRemoving(pDocLayout, pNode);
    XFA_ReleaseLayoutItem(pNode);
    pNode = pNext;
  }
  pNotify->OnLayoutItemRemoving(pDocLayout, pLayoutItem);
  if (pLayoutItem->GetFormNode()->GetElementType() == XFA_Element::PageArea) {
    pNotify->OnPageEvent(ToViewLayoutItem(pLayoutItem),
                         XFA_PAGEVIEWEVENT_PostRemoved);
  }
  delete pLayoutItem;
}

CXFA_LayoutItem::CXFA_LayoutItem(CXFA_Node* pNode, ItemType type)
    : m_ItemType(type), m_pFormNode(pNode) {}

CXFA_LayoutItem::~CXFA_LayoutItem() = default;

CXFA_ViewLayoutItem* CXFA_LayoutItem::AsViewLayoutItem() {
  return IsViewLayoutItem() ? static_cast<CXFA_ViewLayoutItem*>(this) : nullptr;
}

const CXFA_ViewLayoutItem* CXFA_LayoutItem::AsViewLayoutItem() const {
  return IsViewLayoutItem() ? static_cast<const CXFA_ViewLayoutItem*>(this)
                            : nullptr;
}

CXFA_ContentLayoutItem* CXFA_LayoutItem::AsContentLayoutItem() {
  return IsContentLayoutItem() ? static_cast<CXFA_ContentLayoutItem*>(this)
                               : nullptr;
}

const CXFA_ContentLayoutItem* CXFA_LayoutItem::AsContentLayoutItem() const {
  return IsContentLayoutItem()
             ? static_cast<const CXFA_ContentLayoutItem*>(this)
             : nullptr;
}

CXFA_ViewLayoutItem* CXFA_LayoutItem::GetPage() const {
  for (CXFA_LayoutItem* pCurNode = const_cast<CXFA_LayoutItem*>(this); pCurNode;
       pCurNode = pCurNode->m_pParent) {
    if (pCurNode->m_pFormNode->GetElementType() == XFA_Element::PageArea)
      return pCurNode->AsViewLayoutItem();
  }
  return nullptr;
}
void CXFA_LayoutItem::AddChild(CXFA_LayoutItem* pChildItem) {
  if (pChildItem->m_pParent)
    pChildItem->m_pParent->RemoveChild(pChildItem);

  pChildItem->m_pParent = this;
  if (!m_pFirstChild) {
    m_pFirstChild = pChildItem;
    return;
  }

  CXFA_LayoutItem* pExistingChildItem = m_pFirstChild;
  while (pExistingChildItem->m_pNextSibling)
    pExistingChildItem = pExistingChildItem->m_pNextSibling;

  pExistingChildItem->m_pNextSibling = pChildItem;
}

void CXFA_LayoutItem::AddHeadChild(CXFA_LayoutItem* pChildItem) {
  if (pChildItem->m_pParent)
    pChildItem->m_pParent->RemoveChild(pChildItem);

  pChildItem->m_pParent = this;
  if (!m_pFirstChild) {
    m_pFirstChild = pChildItem;
    return;
  }

  CXFA_LayoutItem* pExistingChildItem = m_pFirstChild;
  m_pFirstChild = pChildItem;
  m_pFirstChild->m_pNextSibling = pExistingChildItem;
}

void CXFA_LayoutItem::InsertChild(CXFA_LayoutItem* pBeforeItem,
                                  CXFA_LayoutItem* pChildItem) {
  if (pBeforeItem->m_pParent != this)
    return;
  if (pChildItem->m_pParent)
    pChildItem->m_pParent = nullptr;

  pChildItem->m_pParent = this;

  CXFA_LayoutItem* pExistingChildItem = pBeforeItem->m_pNextSibling;
  pBeforeItem->m_pNextSibling = pChildItem;
  pChildItem->m_pNextSibling = pExistingChildItem;
}

void CXFA_LayoutItem::RemoveChild(CXFA_LayoutItem* pChildItem) {
  if (pChildItem->m_pParent != this)
    return;

  if (m_pFirstChild == pChildItem) {
    m_pFirstChild = pChildItem->m_pNextSibling;
  } else {
    CXFA_LayoutItem* pExistingChildItem = m_pFirstChild;
    while (pExistingChildItem &&
           pExistingChildItem->m_pNextSibling != pChildItem) {
      pExistingChildItem = pExistingChildItem->m_pNextSibling;
    }
    if (pExistingChildItem)
      pExistingChildItem->m_pNextSibling = pChildItem->m_pNextSibling;
  }
  pChildItem->m_pNextSibling = nullptr;
  pChildItem->m_pParent = nullptr;
}
