// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_baannothandler.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fpdfdoc/cpdf_interactiveform.h"
#include "core/fxge/cfx_drawutils.h"
#include "fpdfsdk/cpdfsdk_actionhandler.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_baannot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/formfiller/cffl_formfield.h"
#include "public/fpdf_fwlevent.h"
#include "third_party/base/check.h"
#include "third_party/base/containers/contains.h"
#include "third_party/base/notreached.h"

namespace {

void UpdateAnnotRects(CPDFSDK_BAAnnot* pBAAnnot) {
  std::vector<CFX_FloatRect> rects;
  rects.push_back(pBAAnnot->GetRect());
  if (CPDF_Annot* pPopupAnnot = pBAAnnot->GetPDFPopupAnnot())
    rects.push_back(pPopupAnnot->GetRect());

  // Make the rects round up to avoid https://crbug.com/662804
  for (CFX_FloatRect& rect : rects)
    rect.Inflate(1, 1);

  pBAAnnot->GetPageView()->UpdateRects(rects);
}

}  // namespace

CPDFSDK_BAAnnotHandler::CPDFSDK_BAAnnotHandler() = default;

CPDFSDK_BAAnnotHandler::~CPDFSDK_BAAnnotHandler() = default;

bool CPDFSDK_BAAnnotHandler::CanAnswer(CPDFSDK_Annot* pAnnot) {
  return false;
}

std::unique_ptr<CPDFSDK_Annot> CPDFSDK_BAAnnotHandler::NewAnnot(
    CPDF_Annot* pAnnot,
    CPDFSDK_PageView* pPageView) {
  CHECK(pPageView);
  return std::make_unique<CPDFSDK_BAAnnot>(pAnnot, pPageView);
}

void CPDFSDK_BAAnnotHandler::ReleaseAnnot(
    std::unique_ptr<CPDFSDK_Annot> pAnnot) {
  // pAnnot deleted by unique_ptr going out of scope.
}

void CPDFSDK_BAAnnotHandler::OnDraw(CPDFSDK_Annot* pAnnot,
                                    CFX_RenderDevice* pDevice,
                                    const CFX_Matrix& mtUser2Device,
                                    bool bDrawAnnots) {
  if (pAnnot->AsXFAWidget())
    return;

  if (!pAnnot->AsBAAnnot()->IsVisible())
    return;

  const CPDF_Annot::Subtype annot_type = pAnnot->GetAnnotSubtype();
  if (bDrawAnnots && annot_type == CPDF_Annot::Subtype::POPUP) {
    pAnnot->AsBAAnnot()->DrawAppearance(pDevice, mtUser2Device,
                                        CPDF_Annot::AppearanceMode::kNormal);
    return;
  }

  if (is_annotation_focused_ && IsFocusableAnnot(annot_type) &&
      pAnnot == GetFormFillEnvironment()->GetFocusAnnot()) {
    CFX_FloatRect view_bounding_box = GetViewBBox(pAnnot->AsBAAnnot());
    if (view_bounding_box.IsEmpty())
      return;

    view_bounding_box.Normalize();

    CFX_DrawUtils::DrawFocusRect(pDevice, mtUser2Device, view_bounding_box);
  }
}

void CPDFSDK_BAAnnotHandler::OnMouseEnter(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                          Mask<FWL_EVENTFLAG> nFlag) {
  CPDFSDK_BAAnnot* pBAAnnot = pAnnot->AsBAAnnot();
  pBAAnnot->SetOpenState(true);
  UpdateAnnotRects(pBAAnnot);
}

void CPDFSDK_BAAnnotHandler::OnMouseExit(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlag) {
  CPDFSDK_BAAnnot* pBAAnnot = pAnnot->AsBAAnnot();
  pBAAnnot->SetOpenState(false);
  UpdateAnnotRects(pBAAnnot);
}

bool CPDFSDK_BAAnnotHandler::OnLButtonDown(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                           Mask<FWL_EVENTFLAG> nFlags,
                                           const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnLButtonUp(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnLButtonDblClk(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                             Mask<FWL_EVENTFLAG> nFlags,
                                             const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnMouseMove(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnMouseWheel(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                          Mask<FWL_EVENTFLAG> nFlags,
                                          const CFX_PointF& point,
                                          const CFX_Vector& delta) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonDown(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                           Mask<FWL_EVENTFLAG> nFlags,
                                           const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonUp(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlags,
                                         const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnRButtonDblClk(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                             Mask<FWL_EVENTFLAG> nFlags,
                                             const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnChar(CPDFSDK_Annot* pAnnot,
                                    uint32_t nChar,
                                    Mask<FWL_EVENTFLAG> nFlags) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                       FWL_VKEYCODE nKeyCode,
                                       Mask<FWL_EVENTFLAG> nFlag) {
  DCHECK(pAnnot);

  // OnKeyDown() is implemented only for link annotations for now. As
  // OnKeyDown() is implemented for other subtypes, following check should be
  // modified.
  if (nKeyCode != FWL_VKEY_Return ||
      pAnnot->GetAnnotSubtype() != CPDF_Annot::Subtype::LINK) {
    return false;
  }

  CPDFSDK_BAAnnot* ba_annot = pAnnot->AsBAAnnot();
  CPDF_Action action = ba_annot->GetAAction(CPDF_AAction::kKeyStroke);

  if (action.GetDict()) {
    return GetFormFillEnvironment()->GetActionHandler()->DoAction_Link(
        action, CPDF_AAction::kKeyStroke, GetFormFillEnvironment(), nFlag);
  }

  return GetFormFillEnvironment()->GetActionHandler()->DoAction_Destination(
      ba_annot->GetDestination(), GetFormFillEnvironment());
}

bool CPDFSDK_BAAnnotHandler::OnKeyUp(CPDFSDK_Annot* pAnnot,
                                     FWL_VKEYCODE nKeyCode,
                                     Mask<FWL_EVENTFLAG> nFlag) {
  return false;
}

void CPDFSDK_BAAnnotHandler::OnLoad(CPDFSDK_Annot* pAnnot) {}

bool CPDFSDK_BAAnnotHandler::IsFocusableAnnot(
    const CPDF_Annot::Subtype& annot_type) const {
  DCHECK(annot_type != CPDF_Annot::Subtype::WIDGET);

  return pdfium::Contains(GetFormFillEnvironment()->GetFocusableAnnotSubtypes(),
                          annot_type);
}

void CPDFSDK_BAAnnotHandler::InvalidateRect(CPDFSDK_Annot* annot) {
  CPDFSDK_BAAnnot* ba_annot = annot->AsBAAnnot();
  CFX_FloatRect view_bounding_box = GetViewBBox(ba_annot);
  if (!view_bounding_box.IsEmpty()) {
    view_bounding_box.Inflate(1, 1);
    view_bounding_box.Normalize();
    FX_RECT rect = view_bounding_box.GetOuterRect();
    GetFormFillEnvironment()->Invalidate(ba_annot->GetPage(), rect);
  }
}

bool CPDFSDK_BAAnnotHandler::OnSetFocus(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlag) {
  if (!IsFocusableAnnot(pAnnot->GetAnnotSubtype()))
    return false;

  is_annotation_focused_ = true;
  InvalidateRect(pAnnot.Get());
  return true;
}

bool CPDFSDK_BAAnnotHandler::OnKillFocus(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlag) {
  if (!IsFocusableAnnot(pAnnot->GetAnnotSubtype()))
    return false;

  is_annotation_focused_ = false;
  InvalidateRect(pAnnot.Get());
  return true;
}

bool CPDFSDK_BAAnnotHandler::SetIndexSelected(
    ObservedPtr<CPDFSDK_Annot>& pAnnot,
    int index,
    bool selected) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::IsIndexSelected(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                             int index) {
  return false;
}

CFX_FloatRect CPDFSDK_BAAnnotHandler::GetViewBBox(CPDFSDK_Annot* pAnnot) {
  return pAnnot->GetRect();
}

WideString CPDFSDK_BAAnnotHandler::GetText(CPDFSDK_Annot* pAnnot) {
  return WideString();
}

WideString CPDFSDK_BAAnnotHandler::GetSelectedText(CPDFSDK_Annot* pAnnot) {
  return WideString();
}

void CPDFSDK_BAAnnotHandler::ReplaceSelection(CPDFSDK_Annot* pAnnot,
                                              const WideString& text) {}

bool CPDFSDK_BAAnnotHandler::SelectAllText(CPDFSDK_Annot* pAnnot) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::CanUndo(CPDFSDK_Annot* pAnnot) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::CanRedo(CPDFSDK_Annot* pAnnot) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::Undo(CPDFSDK_Annot* pAnnot) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::Redo(CPDFSDK_Annot* pAnnot) {
  return false;
}

bool CPDFSDK_BAAnnotHandler::HitTest(CPDFSDK_Annot* pAnnot,
                                     const CFX_PointF& point) {
  DCHECK(pAnnot);
  return GetViewBBox(pAnnot).Contains(point);
}

#ifdef PDF_ENABLE_XFA
std::unique_ptr<CPDFSDK_Annot> CPDFSDK_BAAnnotHandler::NewAnnotForXFA(
    CXFA_FFWidget* pWidget,
    CPDFSDK_PageView* pPageView) {
  NOTREACHED();
  return nullptr;
}

bool CPDFSDK_BAAnnotHandler::OnXFAChangedFocus(
    ObservedPtr<CPDFSDK_Annot>& pNewAnnot) {
  NOTREACHED();
  return false;
}
#endif
