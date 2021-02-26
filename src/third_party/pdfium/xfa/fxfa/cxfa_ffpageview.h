// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FFPAGEVIEW_H_
#define XFA_FXFA_CXFA_FFPAGEVIEW_H_

#include <vector>

#include "fxjs/gc/heap.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/cppgc/visitor.h"
#include "xfa/fxfa/fxfa.h"
#include "xfa/fxfa/layout/cxfa_contentlayoutitem.h"
#include "xfa/fxfa/layout/cxfa_traversestrategy_layoutitem.h"
#include "xfa/fxfa/layout/cxfa_viewlayoutitem.h"

class CXFA_FFWidget;
class CXFA_FFDocView;

class CXFA_FFPageView final : public cppgc::GarbageCollected<CXFA_FFPageView> {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FFPageView();

  void Trace(cppgc::Visitor* visitor) const;

  CXFA_ViewLayoutItem* GetLayoutItem() const { return m_pLayoutItem; }
  void SetLayoutItem(CXFA_ViewLayoutItem* pItem) { m_pLayoutItem = pItem; }

  CXFA_FFDocView* GetDocView() const;
  CFX_RectF GetPageViewRect() const;
  CFX_Matrix GetDisplayMatrix(const FX_RECT& rtDisp, int32_t iRotate) const;

  // These always return a non-null iterator from the gc heap.
  IXFA_WidgetIterator* CreateGCedFormWidgetIterator(uint32_t dwWidgetFilter);
  IXFA_WidgetIterator* CreateGCedTraverseWidgetIterator(
      uint32_t dwWidgetFilter);

 private:
  CXFA_FFPageView(CXFA_FFDocView* pDocView, CXFA_Node* pPageArea);

  cppgc::Member<CXFA_Node> const m_pPageArea;
  cppgc::Member<CXFA_FFDocView> const m_pDocView;
  cppgc::Member<CXFA_ViewLayoutItem> m_pLayoutItem;
};

class CXFA_FFPageWidgetIterator final
    : public cppgc::GarbageCollected<CXFA_FFPageWidgetIterator>,
      public IXFA_WidgetIterator {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FFPageWidgetIterator() override;

  void Trace(cppgc::Visitor* visitor) const {}

  void Reset() override;
  CXFA_FFWidget* MoveToFirst() override;
  CXFA_FFWidget* MoveToLast() override;
  CXFA_FFWidget* MoveToNext() override;
  CXFA_FFWidget* MoveToPrevious() override;
  CXFA_FFWidget* GetCurrentWidget() override;
  bool SetCurrentWidget(CXFA_FFWidget* hWidget) override;

 private:
  CXFA_FFPageWidgetIterator(CXFA_FFPageView* pPageView, uint32_t dwFilter);

  CXFA_LayoutItemIterator m_sIterator;
  const uint32_t m_dwFilter;
  const bool m_bIgnoreRelevant;
};

class CXFA_FFTabOrderPageWidgetIterator final
    : public cppgc::GarbageCollected<CXFA_FFPageWidgetIterator>,
      public IXFA_WidgetIterator {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FFTabOrderPageWidgetIterator() override;

  void Trace(cppgc::Visitor* visitor) const;

  void Reset() override;
  CXFA_FFWidget* MoveToFirst() override;
  CXFA_FFWidget* MoveToLast() override;
  CXFA_FFWidget* MoveToNext() override;
  CXFA_FFWidget* MoveToPrevious() override;
  CXFA_FFWidget* GetCurrentWidget() override;
  bool SetCurrentWidget(CXFA_FFWidget* hWidget) override;

 private:
  CXFA_FFTabOrderPageWidgetIterator(CXFA_FFPageView* pPageView,
                                    uint32_t dwFilter);

  CXFA_FFWidget* GetTraverseWidget(CXFA_FFWidget* pWidget);
  CXFA_FFWidget* FindWidgetByName(const WideString& wsWidgetName,
                                  CXFA_FFWidget* pRefWidget);
  void CreateTabOrderWidgetArray();
  std::vector<CXFA_ContentLayoutItem*> CreateSpaceOrderLayoutItems();

  cppgc::Member<CXFA_ViewLayoutItem> const m_pPageViewLayout;
  std::vector<cppgc::Member<CXFA_ContentLayoutItem>> m_TabOrderWidgetArray;
  const uint32_t m_dwFilter;
  int32_t m_iCurWidget = -1;
  const bool m_bIgnoreRelevant;
};

#endif  // XFA_FXFA_CXFA_FFPAGEVIEW_H_
