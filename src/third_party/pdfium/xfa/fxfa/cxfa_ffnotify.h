// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FFNOTIFY_H_
#define XFA_FXFA_CXFA_FFNOTIFY_H_

#include "fxjs/gc/heap.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/cppgc/visitor.h"
#include "xfa/fxfa/cxfa_eventparam.h"
#include "xfa/fxfa/fxfa.h"
#include "xfa/fxfa/parser/cxfa_document.h"

class CXFA_ContentLayoutItem;
class CXFA_FFWidgetHandler;
class CXFA_LayoutItem;
class CXFA_LayoutProcessor;
class CXFA_Script;
class CXFA_ViewLayoutItem;

class CXFA_FFNotify : public cppgc::GarbageCollected<CXFA_FFNotify> {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FFNotify();

  void Trace(cppgc::Visitor* visitor) const;

  void OnPageEvent(CXFA_ViewLayoutItem* pSender, uint32_t dwEvent);

  void OnWidgetListItemAdded(CXFA_Node* pSender,
                             const WideString& wsLabel,
                             int32_t iIndex);
  void OnWidgetListItemRemoved(CXFA_Node* pSender, int32_t iIndex);

  // Node events
  void OnNodeReady(CXFA_Node* pNode);
  void OnValueChanging(CXFA_Node* pSender, XFA_Attribute eAttr);
  void OnValueChanged(CXFA_Node* pSender,
                      XFA_Attribute eAttr,
                      CXFA_Node* pParentNode,
                      CXFA_Node* pWidgetNode);
  void OnContainerChanged(CXFA_Node* pNode);
  void OnChildAdded(CXFA_Node* pSender);
  void OnChildRemoved();

  // These two return new views/widgets from cppgc heap.
  CXFA_FFPageView* OnCreateViewLayoutItem(CXFA_Node* pNode);
  CXFA_FFWidget* OnCreateContentLayoutItem(CXFA_Node* pNode);

  void OnLayoutItemAdded(CXFA_LayoutProcessor* pLayout,
                         CXFA_LayoutItem* pSender,
                         int32_t iPageIdx,
                         uint32_t dwStatus);
  void OnLayoutItemRemoving(CXFA_LayoutProcessor* pLayout,
                            CXFA_LayoutItem* pSender);
  void StartFieldDrawLayout(CXFA_Node* pItem,
                            float* pCalcWidth,
                            float* pCalcHeight);
  bool RunScript(CXFA_Script* pScript, CXFA_Node* pFormItem);
  XFA_EventError ExecEventByDeepFirst(CXFA_Node* pFormNode,
                                      XFA_EVENTTYPE eEventType,
                                      bool bIsFormReady,
                                      bool bRecursive);
  void AddCalcValidate(CXFA_Node* pNode);
  CXFA_FFDoc* GetFFDoc() const { return m_pDoc.Get(); }
  IXFA_AppProvider* GetAppProvider();
  CXFA_FFWidgetHandler* GetWidgetHandler();
  void OpenDropDownList(CXFA_Node* pNode);
  void ResetData(CXFA_Node* pNode);
  int32_t GetLayoutStatus();
  void RunNodeInitialize(CXFA_Node* pNode);
  void RunSubformIndexChange(CXFA_Node* pSubformNode);
  CXFA_Node* GetFocusWidgetNode();
  void SetFocusWidgetNode(CXFA_Node* pNode);

 private:
  explicit CXFA_FFNotify(CXFA_FFDoc* pDoc);

  cppgc::Member<CXFA_FFDoc> const m_pDoc;
};

#endif  // XFA_FXFA_CXFA_FFNOTIFY_H_
