// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_IPDFSDK_ANNOTHANDLER_H_
#define FPDFSDK_IPDFSDK_ANNOTHANDLER_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"
#include "fpdfsdk/cpdfsdk_annot.h"

class CFX_Matrix;
class CFX_RenderDevice;
class CPDF_Annot;
class CPDFSDK_FormFillEnvironment;
class CPDFSDK_PageView;

class IPDFSDK_AnnotHandler {
 public:
  virtual ~IPDFSDK_AnnotHandler() = default;

  virtual void SetFormFillEnvironment(
      CPDFSDK_FormFillEnvironment* pFormFillEnv) = 0;
  virtual bool CanAnswer(CPDFSDK_Annot* pAnnot) = 0;
  virtual std::unique_ptr<CPDFSDK_Annot> NewAnnot(
      CPDF_Annot* pAnnot,
      CPDFSDK_PageView* pPageView) = 0;
  virtual void ReleaseAnnot(std::unique_ptr<CPDFSDK_Annot> pAnnot) = 0;
  virtual CFX_FloatRect GetViewBBox(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot) = 0;
  virtual WideString GetText(CPDFSDK_Annot* pAnnot) = 0;
  virtual WideString GetSelectedText(CPDFSDK_Annot* pAnnot) = 0;
  virtual void ReplaceSelection(CPDFSDK_Annot* pAnnot,
                                const WideString& text) = 0;
  virtual bool SelectAllText(CPDFSDK_Annot* pAnnot) = 0;
  virtual bool CanUndo(CPDFSDK_Annot* pAnnot) = 0;
  virtual bool CanRedo(CPDFSDK_Annot* pAnnot) = 0;
  virtual bool Undo(CPDFSDK_Annot* pAnnot) = 0;
  virtual bool Redo(CPDFSDK_Annot* pAnnot) = 0;
  virtual bool HitTest(CPDFSDK_PageView* pPageView,
                       CPDFSDK_Annot* pAnnot,
                       const CFX_PointF& point) = 0;
  virtual void OnDraw(CPDFSDK_PageView* pPageView,
                      CPDFSDK_Annot* pAnnot,
                      CFX_RenderDevice* pDevice,
                      const CFX_Matrix& mtUser2Device,
                      bool bDrawAnnots) = 0;
  virtual void OnLoad(CPDFSDK_Annot* pAnnot) = 0;
  virtual void OnMouseEnter(CPDFSDK_PageView* pPageView,
                            ObservedPtr<CPDFSDK_Annot>* pAnnot,
                            uint32_t nFlag) = 0;
  virtual void OnMouseExit(CPDFSDK_PageView* pPageView,
                           ObservedPtr<CPDFSDK_Annot>* pAnnot,
                           uint32_t nFlag) = 0;
  virtual bool OnLButtonDown(CPDFSDK_PageView* pPageView,
                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                             uint32_t nFlags,
                             const CFX_PointF& point) = 0;
  virtual bool OnLButtonUp(CPDFSDK_PageView* pPageView,
                           ObservedPtr<CPDFSDK_Annot>* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                               ObservedPtr<CPDFSDK_Annot>* pAnnot,
                               uint32_t nFlags,
                               const CFX_PointF& point) = 0;
  virtual bool OnMouseMove(CPDFSDK_PageView* pPageView,
                           ObservedPtr<CPDFSDK_Annot>* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnMouseWheel(CPDFSDK_PageView* pPageView,
                            ObservedPtr<CPDFSDK_Annot>* pAnnot,
                            uint32_t nFlags,
                            const CFX_PointF& point,
                            const CFX_Vector& delta) = 0;
  virtual bool OnRButtonDown(CPDFSDK_PageView* pPageView,
                             ObservedPtr<CPDFSDK_Annot>* pAnnot,
                             uint32_t nFlags,
                             const CFX_PointF& point) = 0;
  virtual bool OnRButtonUp(CPDFSDK_PageView* pPageView,
                           ObservedPtr<CPDFSDK_Annot>* pAnnot,
                           uint32_t nFlags,
                           const CFX_PointF& point) = 0;
  virtual bool OnRButtonDblClk(CPDFSDK_PageView* pPageView,
                               ObservedPtr<CPDFSDK_Annot>* pAnnot,
                               uint32_t nFlags,
                               const CFX_PointF& point) = 0;
  virtual bool OnChar(CPDFSDK_Annot* pAnnot,
                      uint32_t nChar,
                      uint32_t nFlags) = 0;
  virtual bool OnKeyDown(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) = 0;
  virtual bool OnKeyUp(CPDFSDK_Annot* pAnnot, int nKeyCode, int nFlag) = 0;
  virtual bool OnSetFocus(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                          uint32_t nFlag) = 0;
  virtual bool OnKillFocus(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                           uint32_t nFlag) = 0;
  virtual bool SetIndexSelected(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                int index,
                                bool selected) = 0;
  virtual bool IsIndexSelected(ObservedPtr<CPDFSDK_Annot>* pAnnot,
                               int index) = 0;
};

#endif  // FPDFSDK_IPDFSDK_ANNOTHANDLER_H_
