// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_attachnodelist.h"

#include "third_party/base/numerics/safe_conversions.h"
#include "xfa/fxfa/parser/cxfa_node.h"

CXFA_AttachNodeList::CXFA_AttachNodeList(CXFA_Document* pDocument,
                                         CXFA_Node* pAttachNode)
    : CXFA_TreeList(pDocument), m_pAttachNode(pAttachNode) {}

CXFA_AttachNodeList::~CXFA_AttachNodeList() = default;

void CXFA_AttachNodeList::Trace(cppgc::Visitor* visitor) const {
  CXFA_TreeList::Trace(visitor);
  visitor->Trace(m_pAttachNode);
}

size_t CXFA_AttachNodeList::GetLength() {
  return m_pAttachNode->CountChildren(
      XFA_Element::Unknown,
      m_pAttachNode->GetElementType() == XFA_Element::Subform);
}

bool CXFA_AttachNodeList::Append(CXFA_Node* pNode) {
  if (pNode->IsAncestorOf(m_pAttachNode))
    return false;

  CXFA_Node* pParent = pNode->GetParent();
  if (pParent)
    pParent->RemoveChildAndNotify(pNode, true);

  m_pAttachNode->InsertChildAndNotify(pNode, nullptr);
  return true;
}

bool CXFA_AttachNodeList::Insert(CXFA_Node* pNewNode, CXFA_Node* pBeforeNode) {
  if (pNewNode->IsAncestorOf(m_pAttachNode))
    return false;

  if (pBeforeNode && pBeforeNode->GetParent() != m_pAttachNode)
    return false;

  CXFA_Node* pParent = pNewNode->GetParent();
  if (pParent)
    pParent->RemoveChildAndNotify(pNewNode, true);

  m_pAttachNode->InsertChildAndNotify(pNewNode, pBeforeNode);
  return true;
}

void CXFA_AttachNodeList::Remove(CXFA_Node* pNode) {
  m_pAttachNode->RemoveChildAndNotify(pNode, true);
}

CXFA_Node* CXFA_AttachNodeList::Item(size_t index) {
  return m_pAttachNode->GetChild<CXFA_Node>(
      index, XFA_Element::Unknown,
      m_pAttachNode->GetElementType() == XFA_Element::Subform);
}
