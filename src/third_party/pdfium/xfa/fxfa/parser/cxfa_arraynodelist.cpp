// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_arraynodelist.h"

#include <utility>
#include <vector>

#include "fxjs/gc/container_trace.h"
#include "xfa/fxfa/parser/cxfa_node.h"

CXFA_ArrayNodeList::CXFA_ArrayNodeList(CXFA_Document* pDocument)
    : CXFA_TreeList(pDocument) {}

CXFA_ArrayNodeList::~CXFA_ArrayNodeList() = default;

void CXFA_ArrayNodeList::Trace(cppgc::Visitor* visitor) const {
  CXFA_TreeList::Trace(visitor);
  ContainerTrace(visitor, m_array);
}

void CXFA_ArrayNodeList::SetArrayNodeList(
    const std::vector<CXFA_Node*>& srcArray) {
  if (!srcArray.empty()) {
    m_array =
        std::vector<cppgc::Member<CXFA_Node>>(srcArray.begin(), srcArray.end());
  }
}

size_t CXFA_ArrayNodeList::GetLength() {
  return m_array.size();
}

bool CXFA_ArrayNodeList::Append(CXFA_Node* pNode) {
  m_array.push_back(pNode);
  return true;
}

bool CXFA_ArrayNodeList::Insert(CXFA_Node* pNewNode, CXFA_Node* pBeforeNode) {
  if (!pBeforeNode) {
    m_array.push_back(pNewNode);
    return true;
  }

  auto it = std::find(m_array.begin(), m_array.end(), pBeforeNode);
  if (it == m_array.end())
    return false;

  m_array.insert(it, pNewNode);
  return true;
}

void CXFA_ArrayNodeList::Remove(CXFA_Node* pNode) {
  auto it = std::find(m_array.begin(), m_array.end(), pNode);
  if (it != m_array.end())
    m_array.erase(it);
}

CXFA_Node* CXFA_ArrayNodeList::Item(size_t index) {
  return index < m_array.size() ? m_array[index] : nullptr;
}
