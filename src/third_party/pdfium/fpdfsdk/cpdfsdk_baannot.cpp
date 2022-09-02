// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_baannot.h"

#include <vector>

#include "constants/annotation_common.h"
#include "constants/annotation_flags.h"
#include "constants/form_fields.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fxge/cfx_drawutils.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "third_party/base/check.h"
#include "third_party/base/containers/contains.h"

CPDFSDK_BAAnnot::CPDFSDK_BAAnnot(CPDF_Annot* pAnnot,
                                 CPDFSDK_PageView* pPageView)
    : CPDFSDK_Annot(pPageView), m_pAnnot(pAnnot) {}

CPDFSDK_BAAnnot::~CPDFSDK_BAAnnot() = default;

CPDFSDK_BAAnnot* CPDFSDK_BAAnnot::AsBAAnnot() {
  return this;
}

CPDFSDK_Annot::UnsafeInputHandlers* CPDFSDK_BAAnnot::GetUnsafeInputHandlers() {
  return this;
}

CPDF_Annot* CPDFSDK_BAAnnot::GetPDFAnnot() const {
  return m_pAnnot.Get();
}

CPDF_Annot* CPDFSDK_BAAnnot::GetPDFPopupAnnot() const {
  return m_pAnnot->GetPopupAnnot();
}

CPDF_Dictionary* CPDFSDK_BAAnnot::GetAnnotDict() const {
  return m_pAnnot->GetAnnotDict();
}

CPDF_Dictionary* CPDFSDK_BAAnnot::GetAPDict() const {
  return GetOrCreateDict(GetAnnotDict(), pdfium::annotation::kAP);
}

void CPDFSDK_BAAnnot::ClearCachedAnnotAP() {
  m_pAnnot->ClearCachedAP();
}

bool CPDFSDK_BAAnnot::IsFocusableAnnot(
    const CPDF_Annot::Subtype& annot_type) const {
  return pdfium::Contains(
      m_pPageView->GetFormFillEnv()->GetFocusableAnnotSubtypes(), annot_type);
}

CFX_FloatRect CPDFSDK_BAAnnot::GetRect() const {
  return m_pAnnot->GetRect();
}

CPDF_Annot::Subtype CPDFSDK_BAAnnot::GetAnnotSubtype() const {
  return m_pAnnot->GetSubtype();
}

void CPDFSDK_BAAnnot::DrawAppearance(CFX_RenderDevice* pDevice,
                                     const CFX_Matrix& mtUser2Device,
                                     CPDF_Annot::AppearanceMode mode) {
  m_pAnnot->DrawAppearance(m_pPageView->GetPDFPage(), pDevice, mtUser2Device,
                           mode);
}

bool CPDFSDK_BAAnnot::IsAppearanceValid() {
  return !!GetAnnotDict()->GetDictFor(pdfium::annotation::kAP);
}

void CPDFSDK_BAAnnot::SetAnnotName(const WideString& sName) {
  CPDF_Dictionary* pDict = GetAnnotDict();
  if (sName.IsEmpty()) {
    pDict->RemoveFor(pdfium::annotation::kNM);
    return;
  }
  pDict->SetNewFor<CPDF_String>(pdfium::annotation::kNM, sName.AsStringView());
}

WideString CPDFSDK_BAAnnot::GetAnnotName() const {
  return GetAnnotDict()->GetUnicodeTextFor(pdfium::annotation::kNM);
}

void CPDFSDK_BAAnnot::SetFlags(uint32_t nFlags) {
  GetAnnotDict()->SetNewFor<CPDF_Number>(pdfium::annotation::kF,
                                         static_cast<int>(nFlags));
}

uint32_t CPDFSDK_BAAnnot::GetFlags() const {
  return GetAnnotDict()->GetIntegerFor(pdfium::annotation::kF);
}

void CPDFSDK_BAAnnot::SetAppStateOff() {
  CPDF_Dictionary* pDict = GetAnnotDict();
  pDict->SetNewFor<CPDF_String>(pdfium::annotation::kAS, "Off", false);
}

ByteString CPDFSDK_BAAnnot::GetAppState() const {
  return GetAnnotDict()->GetStringFor(pdfium::annotation::kAS);
}

void CPDFSDK_BAAnnot::SetBorderWidth(int nWidth) {
  CPDF_Array* pBorder =
      GetAnnotDict()->GetArrayFor(pdfium::annotation::kBorder);
  if (pBorder) {
    pBorder->SetNewAt<CPDF_Number>(2, nWidth);
  } else {
    CPDF_Dictionary* pBSDict = GetOrCreateDict(GetAnnotDict(), "BS");
    pBSDict->SetNewFor<CPDF_Number>("W", nWidth);
  }
}

int CPDFSDK_BAAnnot::GetBorderWidth() const {
  if (const CPDF_Array* pBorder =
          GetAnnotDict()->GetArrayFor(pdfium::annotation::kBorder)) {
    return pBorder->GetIntegerAt(2);
  }

  if (CPDF_Dictionary* pBSDict = GetAnnotDict()->GetDictFor("BS"))
    return pBSDict->GetIntegerFor("W", 1);

  return 1;
}

void CPDFSDK_BAAnnot::SetBorderStyle(BorderStyle nStyle) {
  CPDF_Dictionary* pBSDict = GetOrCreateDict(GetAnnotDict(), "BS");
  const char* name = nullptr;
  switch (nStyle) {
    case BorderStyle::kSolid:
      name = "S";
      break;
    case BorderStyle::kDash:
      name = "D";
      break;
    case BorderStyle::kBeveled:
      name = "B";
      break;
    case BorderStyle::kInset:
      name = "I";
      break;
    case BorderStyle::kUnderline:
      name = "U";
      break;
    default:
      return;
  }
  pBSDict->SetNewFor<CPDF_Name>("S", name);
}

BorderStyle CPDFSDK_BAAnnot::GetBorderStyle() const {
  CPDF_Dictionary* pBSDict = GetAnnotDict()->GetDictFor("BS");
  if (pBSDict) {
    ByteString sBorderStyle = pBSDict->GetStringFor("S", "S");
    if (sBorderStyle == "S")
      return BorderStyle::kSolid;
    if (sBorderStyle == "D")
      return BorderStyle::kDash;
    if (sBorderStyle == "B")
      return BorderStyle::kBeveled;
    if (sBorderStyle == "I")
      return BorderStyle::kInset;
    if (sBorderStyle == "U")
      return BorderStyle::kUnderline;
  }

  const CPDF_Array* pBorder =
      GetAnnotDict()->GetArrayFor(pdfium::annotation::kBorder);
  if (pBorder) {
    if (pBorder->size() >= 4) {
      const CPDF_Array* pDP = pBorder->GetArrayAt(3);
      if (pDP && pDP->size() > 0)
        return BorderStyle::kDash;
    }
  }

  return BorderStyle::kSolid;
}

bool CPDFSDK_BAAnnot::IsVisible() const {
  uint32_t nFlags = GetFlags();
  return !((nFlags & pdfium::annotation_flags::kInvisible) ||
           (nFlags & pdfium::annotation_flags::kHidden) ||
           (nFlags & pdfium::annotation_flags::kNoView));
}

CPDF_Action CPDFSDK_BAAnnot::GetAction() const {
  return CPDF_Action(GetAnnotDict()->GetDictFor("A"));
}

CPDF_AAction CPDFSDK_BAAnnot::GetAAction() const {
  return CPDF_AAction(GetAnnotDict()->GetDictFor(pdfium::form_fields::kAA));
}

CPDF_Action CPDFSDK_BAAnnot::GetAAction(CPDF_AAction::AActionType eAAT) {
  CPDF_AAction AAction = GetAAction();
  if (AAction.ActionExist(eAAT))
    return AAction.GetAction(eAAT);

  if (eAAT == CPDF_AAction::kButtonUp || eAAT == CPDF_AAction::kKeyStroke)
    return GetAction();

  return CPDF_Action(nullptr);
}

void CPDFSDK_BAAnnot::SetOpenState(bool bOpenState) {
  if (CPDF_Annot* pAnnot = m_pAnnot->GetPopupAnnot())
    pAnnot->SetOpenState(bOpenState);
}

void CPDFSDK_BAAnnot::UpdateAnnotRects() {
  std::vector<CFX_FloatRect> rects;
  rects.push_back(GetRect());
  CPDF_Annot* popup = GetPDFPopupAnnot();
  if (popup)
    rects.push_back(popup->GetRect());

  // Make the rects round up to avoid https://crbug.com/662804
  for (CFX_FloatRect& rect : rects)
    rect.Inflate(1, 1);

  GetPageView()->UpdateRects(rects);
}

void CPDFSDK_BAAnnot::InvalidateRect() {
  CFX_FloatRect view_bounding_box = GetViewBBox();
  if (view_bounding_box.IsEmpty())
    return;

  view_bounding_box.Inflate(1, 1);
  view_bounding_box.Normalize();
  FX_RECT rect = view_bounding_box.GetOuterRect();
  m_pPageView->GetFormFillEnv()->Invalidate(GetPage(), rect);
}

int CPDFSDK_BAAnnot::GetLayoutOrder() const {
  if (m_pAnnot->GetSubtype() == CPDF_Annot::Subtype::POPUP)
    return 1;

  return CPDFSDK_Annot::GetLayoutOrder();
}

void CPDFSDK_BAAnnot::OnDraw(CFX_RenderDevice* pDevice,
                             const CFX_Matrix& mtUser2Device,
                             bool bDrawAnnots) {
  if (!IsVisible())
    return;

  const CPDF_Annot::Subtype annot_type = GetAnnotSubtype();
  if (bDrawAnnots && annot_type == CPDF_Annot::Subtype::POPUP) {
    DrawAppearance(pDevice, mtUser2Device, CPDF_Annot::AppearanceMode::kNormal);
    return;
  }

  if (!is_focused_ || !IsFocusableAnnot(annot_type) ||
      this != m_pPageView->GetFormFillEnv()->GetFocusAnnot()) {
    return;
  }

  CFX_FloatRect view_bounding_box = GetViewBBox();
  if (view_bounding_box.IsEmpty())
    return;

  view_bounding_box.Normalize();
  CFX_DrawUtils::DrawFocusRect(pDevice, mtUser2Device, view_bounding_box);
}

bool CPDFSDK_BAAnnot::DoHitTest(const CFX_PointF& point) {
  return false;
}

CFX_FloatRect CPDFSDK_BAAnnot::GetViewBBox() {
  return GetRect();
}

void CPDFSDK_BAAnnot::OnMouseEnter(Mask<FWL_EVENTFLAG> nFlags) {
  SetOpenState(true);
  UpdateAnnotRects();
}

void CPDFSDK_BAAnnot::OnMouseExit(Mask<FWL_EVENTFLAG> nFlags) {
  SetOpenState(false);
  UpdateAnnotRects();
}

bool CPDFSDK_BAAnnot::OnLButtonDown(Mask<FWL_EVENTFLAG> nFlags,
                                    const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnLButtonUp(Mask<FWL_EVENTFLAG> nFlags,
                                  const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnLButtonDblClk(Mask<FWL_EVENTFLAG> nFlags,
                                      const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnMouseMove(Mask<FWL_EVENTFLAG> nFlags,
                                  const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnMouseWheel(Mask<FWL_EVENTFLAG> nFlags,
                                   const CFX_PointF& point,
                                   const CFX_Vector& delta) {
  return false;
}

bool CPDFSDK_BAAnnot::OnRButtonDown(Mask<FWL_EVENTFLAG> nFlags,
                                    const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnRButtonUp(Mask<FWL_EVENTFLAG> nFlags,
                                  const CFX_PointF& point) {
  return false;
}

bool CPDFSDK_BAAnnot::OnChar(uint32_t nChar, Mask<FWL_EVENTFLAG> nFlags) {
  return false;
}

bool CPDFSDK_BAAnnot::OnKeyDown(FWL_VKEYCODE nKeyCode,
                                Mask<FWL_EVENTFLAG> nFlags) {
  // OnKeyDown() is implemented only for link annotations for now. As
  // OnKeyDown() is implemented for other subtypes, following check should be
  // modified.
  if (nKeyCode != FWL_VKEY_Return ||
      GetAnnotSubtype() != CPDF_Annot::Subtype::LINK) {
    return false;
  }

  CPDF_Action action = GetAAction(CPDF_AAction::kKeyStroke);
  CPDFSDK_FormFillEnvironment* env = m_pPageView->GetFormFillEnv();
  if (action.GetDict()) {
    return env->DoActionLink(action, CPDF_AAction::kKeyStroke, nFlags);
  }

  return env->DoActionDestination(GetDestination());
}

bool CPDFSDK_BAAnnot::OnSetFocus(Mask<FWL_EVENTFLAG> nFlags) {
  if (!IsFocusableAnnot(GetAnnotSubtype()))
    return false;

  is_focused_ = true;
  InvalidateRect();
  return true;
}

bool CPDFSDK_BAAnnot::OnKillFocus(Mask<FWL_EVENTFLAG> nFlags) {
  if (!IsFocusableAnnot(GetAnnotSubtype()))
    return false;

  is_focused_ = false;
  InvalidateRect();
  return true;
}

bool CPDFSDK_BAAnnot::CanUndo() {
  return false;
}

bool CPDFSDK_BAAnnot::CanRedo() {
  return false;
}

bool CPDFSDK_BAAnnot::Undo() {
  return false;
}

bool CPDFSDK_BAAnnot::Redo() {
  return false;
}

WideString CPDFSDK_BAAnnot::GetText() {
  return WideString();
}

WideString CPDFSDK_BAAnnot::GetSelectedText() {
  return WideString();
}

void CPDFSDK_BAAnnot::ReplaceSelection(const WideString& text) {}

bool CPDFSDK_BAAnnot::SelectAllText() {
  return false;
}

bool CPDFSDK_BAAnnot::SetIndexSelected(int index, bool selected) {
  return false;
}

bool CPDFSDK_BAAnnot::IsIndexSelected(int index) {
  return false;
}

CPDF_Dest CPDFSDK_BAAnnot::GetDestination() const {
  if (m_pAnnot->GetSubtype() != CPDF_Annot::Subtype::LINK)
    return CPDF_Dest(nullptr);

  // Link annotations can have "Dest" entry defined as an explicit array.
  // See ISO 32000-1:2008 spec, section 12.3.2.1.
  return CPDF_Dest::Create(m_pPageView->GetPDFDocument(),
                           GetAnnotDict()->GetDirectObjectFor("Dest"));
}
