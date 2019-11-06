// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_LAYOUT_CXFA_ITEMLAYOUTPROCESSOR_H_
#define XFA_FXFA_LAYOUT_CXFA_ITEMLAYOUTPROCESSOR_H_

#include <float.h>

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/unowned_ptr.h"
#include "third_party/base/optional.h"
#include "xfa/fxfa/fxfa_basic.h"

constexpr float kXFALayoutPrecision = 0.0005f;

class CXFA_ContentLayoutItem;
class CXFA_ItemLayoutProcessor;
class CXFA_LayoutContext;
class CXFA_LayoutPageMgr;
class CXFA_LayoutProcessor;
class CXFA_Node;
class CXFA_ViewLayoutItem;

class CXFA_ItemLayoutProcessor {
 public:
  enum class Result : uint8_t {
    kDone,
    kPageFullBreak,
    kRowFullBreak,
    kManualBreak,
  };

  enum class Stage : uint8_t {
    kNone,
    kBookendLeader,
    kBreakBefore,
    kKeep,
    kContainer,
    kBreakAfter,
    kBookendTrailer,
    kDone,
  };

  CXFA_ItemLayoutProcessor(CXFA_Node* pNode, CXFA_LayoutPageMgr* pPageMgr);
  ~CXFA_ItemLayoutProcessor();

  Result DoLayout(bool bUseBreakControl,
                  float fHeightLimit,
                  float fRealHeight,
                  CXFA_LayoutContext* pContext);
  void DoLayoutPageArea(CXFA_ViewLayoutItem* pPageAreaLayoutItem);

  CXFA_Node* GetFormNode() { return m_pFormNode; }
  CXFA_ContentLayoutItem* ExtractLayoutItem();

 private:
  CFX_SizeF GetCurrentComponentSize();
  bool HasLayoutItem() const { return !!m_pLayoutItem; }
  void SplitLayoutItem(float fSplitPos);

  float FindSplitPos(float fProposedSplitPos);

  bool ProcessKeepForSplit(
      CXFA_ItemLayoutProcessor* pChildProcessor,
      Result eRetValue,
      std::vector<CXFA_ContentLayoutItem*>* rgCurLineLayoutItem,
      float* fContentCurRowAvailWidth,
      float* fContentCurRowHeight,
      float* fContentCurRowY,
      bool* bAddedItemInRow,
      bool* bForceEndPage,
      Result* result);
  void ProcessUnUseOverFlow(CXFA_Node* pLeaderNode,
                            CXFA_Node* pTrailerNode,
                            CXFA_ContentLayoutItem* pTrailerItem,
                            CXFA_Node* pFormNode);
  bool IsAddNewRowForTrailer(CXFA_ContentLayoutItem* pTrailerItem);
  bool JudgeLeaderOrTrailerForOccur(CXFA_Node* pFormNode);

  CXFA_ContentLayoutItem* CreateContentLayoutItem(CXFA_Node* pFormNode);

  void SetCurrentComponentPos(const CFX_PointF& pos);
  void SetCurrentComponentSize(const CFX_SizeF& size);

  void SplitLayoutItem(CXFA_ContentLayoutItem* pLayoutItem,
                       CXFA_ContentLayoutItem* pSecondParent,
                       float fSplitPos);
  float InsertKeepLayoutItems();
  bool CalculateRowChildPosition(
      std::vector<CXFA_ContentLayoutItem*> (&rgCurLineLayoutItems)[3],
      XFA_AttributeValue eFlowStrategy,
      bool bContainerHeightAutoSize,
      bool bContainerWidthAutoSize,
      float* fContentCalculatedWidth,
      float* fContentCalculatedHeight,
      float* fContentCurRowY,
      float fContentCurRowHeight,
      float fContentWidthLimit,
      bool bRootForceTb);
  void ProcessUnUseBinds(CXFA_Node* pFormNode);
  bool JudgePutNextPage(CXFA_ContentLayoutItem* pParentLayoutItem,
                        float fChildHeight,
                        std::vector<CXFA_ContentLayoutItem*>* pKeepItems);

  void DoLayoutPositionedContainer(CXFA_LayoutContext* pContext);
  void DoLayoutTableContainer(CXFA_Node* pLayoutNode);
  Result DoLayoutFlowedContainer(bool bUseBreakControl,
                                 XFA_AttributeValue eFlowStrategy,
                                 float fHeightLimit,
                                 float fRealHeight,
                                 CXFA_LayoutContext* pContext,
                                 bool bRootForceTb);
  void DoLayoutField();

  void GotoNextContainerNodeSimple(bool bUsePageBreak);
  Stage GotoNextContainerNode(Stage nCurStage,
                              bool bUsePageBreak,
                              CXFA_Node* pParentContainer,
                              CXFA_Node** pCurActionNode);

  Optional<Stage> ProcessKeepNodesForCheckNext(CXFA_Node** pCurActionNode,
                                               CXFA_Node** pNextContainer,
                                               bool* pLastKeepNode);

  Optional<Stage> ProcessKeepNodesForBreakBefore(CXFA_Node** pCurActionNode,
                                                 CXFA_Node* pContainerNode);

  CXFA_Node* GetSubformSetParent(CXFA_Node* pSubformSet);

  void UpdatePendingItemLayout(CXFA_ContentLayoutItem* pLayoutItem);
  void AddTrailerBeforeSplit(float fSplitPos,
                             CXFA_ContentLayoutItem* pTrailerLayoutItem,
                             bool bUseInherited);
  void AddLeaderAfterSplit(CXFA_ContentLayoutItem* pLeaderLayoutItem);
  void AddPendingNode(CXFA_Node* pPendingNode, bool bBreakPending);
  float InsertPendingItems(CXFA_Node* pCurChildNode);
  Result InsertFlowedItem(
      CXFA_ItemLayoutProcessor* pProcessor,
      bool bContainerWidthAutoSize,
      bool bContainerHeightAutoSize,
      float fContainerHeight,
      XFA_AttributeValue eFlowStrategy,
      uint8_t* uCurHAlignState,
      std::vector<CXFA_ContentLayoutItem*> (&rgCurLineLayoutItems)[3],
      bool bUseBreakControl,
      float fAvailHeight,
      float fRealHeight,
      float fContentWidthLimit,
      float* fContentCurRowY,
      float* fContentCurRowAvailWidth,
      float* fContentCurRowHeight,
      bool* bAddedItemInRow,
      bool* bForceEndPage,
      CXFA_LayoutContext* pLayoutContext,
      bool bNewRow);

  Optional<Stage> HandleKeep(CXFA_Node* pBreakAfterNode,
                             CXFA_Node** pCurActionNode);
  Optional<Stage> HandleBookendLeader(CXFA_Node* pParentContainer,
                                      CXFA_Node** pCurActionNode);
  Optional<Stage> HandleBreakBefore(CXFA_Node* pChildContainer,
                                    CXFA_Node** pCurActionNode);
  Optional<Stage> HandleBreakAfter(CXFA_Node* pChildContainer,
                                   CXFA_Node** pCurActionNode);
  Optional<Stage> HandleCheckNextChildContainer(CXFA_Node* pParentContainer,
                                                CXFA_Node* pChildContainer,
                                                CXFA_Node** pCurActionNode);
  Optional<Stage> HandleBookendTrailer(CXFA_Node* pParentContainer,
                                       CXFA_Node** pCurActionNode);
  void ProcessKeepNodesEnd();
  void AdjustContainerSpecifiedSize(CXFA_LayoutContext* pContext,
                                    CFX_SizeF* pSize,
                                    bool* pContainerWidthAutoSize,
                                    bool* pContainerHeightAutoSize);
  CXFA_ContentLayoutItem* FindLastContentLayoutItem(
      XFA_AttributeValue eFlowStrategy);
  CFX_SizeF CalculateLayoutItemSize(const CXFA_ContentLayoutItem* pLayoutChild);

  Stage m_nCurChildNodeStage = Stage::kNone;
  Result m_ePreProcessRs = Result::kDone;
  bool m_bBreakPending = true;
  bool m_bUseInherited = false;
  bool m_bKeepBreakFinish = false;
  bool m_bIsProcessKeep = false;
  bool m_bHasAvailHeight = true;
  float m_fUsedSize = 0;
  float m_fLastRowWidth = 0;
  float m_fLastRowY = 0;
  float m_fWidthLimit = 0;
  CXFA_Node* const m_pFormNode;
  CXFA_Node* m_pCurChildNode = nullptr;
  CXFA_Node* m_pKeepHeadNode = nullptr;
  CXFA_Node* m_pKeepTailNode = nullptr;
  CXFA_ContentLayoutItem* m_pLayoutItem = nullptr;
  CXFA_ContentLayoutItem* m_pOldLayoutItem = nullptr;
  UnownedPtr<CXFA_LayoutPageMgr> m_pPageMgr;
  std::vector<float> m_rgSpecifiedColumnWidths;
  std::vector<CXFA_ContentLayoutItem*> m_arrayKeepItems;
  std::list<CXFA_Node*> m_PendingNodes;
  std::map<CXFA_Node*, int32_t> m_PendingNodesCount;
  std::unique_ptr<CXFA_ItemLayoutProcessor> m_pCurChildPreprocessor;
};

#endif  // XFA_FXFA_LAYOUT_CXFA_ITEMLAYOUTPROCESSOR_H_
