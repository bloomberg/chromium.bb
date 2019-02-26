// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_NODEHELPER_H_
#define XFA_FXFA_PARSER_CXFA_NODEHELPER_H_

#include <vector>

#include "core/fxcrt/fx_string.h"
#include "xfa/fxfa/fxfa_basic.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"

class CFXJSE_Engine;
class CXFA_Node;

enum XFA_LOGIC_TYPE {
  XFA_LOGIC_NoTransparent,
  XFA_LOGIC_Transparent,
};

class CXFA_NodeHelper {
 public:
  CXFA_NodeHelper();
  ~CXFA_NodeHelper();

  CXFA_Node* ResolveNodes_GetOneChild(CXFA_Node* parent,
                                      const wchar_t* pwsName,
                                      bool bIsClassName);
  CXFA_Node* ResolveNodes_GetParent(CXFA_Node* pNode,
                                    XFA_LOGIC_TYPE eLogicType);

  int32_t NodeAcc_TraverseSiblings(CXFA_Node* parent,
                                   uint32_t dNameHash,
                                   std::vector<CXFA_Node*>* pSiblings,
                                   XFA_LOGIC_TYPE eLogicType,
                                   bool bIsClassName,
                                   bool bIsFindProperty);
  int32_t NodeAcc_TraverseAnySiblings(CXFA_Node* parent,
                                      uint32_t dNameHash,
                                      std::vector<CXFA_Node*>* pSiblings,
                                      bool bIsClassName);
  int32_t CountSiblings(CXFA_Node* pNode,
                        XFA_LOGIC_TYPE eLogicType,
                        std::vector<CXFA_Node*>* pSiblings,
                        bool bIsClassName);
  int32_t GetIndex(CXFA_Node* pNode,
                   XFA_LOGIC_TYPE eLogicType,
                   bool bIsProperty,
                   bool bIsClassIndex);
  WideString GetNameExpression(CXFA_Node* refNode, bool bIsAllPath);
  bool NodeIsTransparent(CXFA_Node* refNode);
  bool ResolveNodes_CreateNode(WideString wsName,
                               WideString wsCondition,
                               bool bLastNode,
                               CFXJSE_Engine* pScriptContext);
  bool CreateNode_ForCondition(WideString& wsCondition);
  void SetCreateNodeType(CXFA_Node* refNode);
  bool NodeIsProperty(CXFA_Node* refNode);

  XFA_Element m_eLastCreateType = XFA_Element::DataValue;
  XFA_ResolveNode_RSType m_iCreateFlag = XFA_ResolveNode_RSType_CreateNodeOne;
  int32_t m_iCreateCount = 0;
  int32_t m_iCurAllStart = -1;
  UnownedPtr<CXFA_Node> m_pCreateParent;
  UnownedPtr<CXFA_Node> m_pAllStartParent;
};

#endif  // XFA_FXFA_PARSER_CXFA_NODEHELPER_H_
