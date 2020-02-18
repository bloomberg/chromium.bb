// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/formfiller/cffl_formfiller.h"

#include <utility>

#include "constants/form_flags.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/cpdfsdk_widget.h"

CFFL_FormFiller::CFFL_FormFiller(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                 CPDFSDK_Widget* pWidget)
    : m_pFormFillEnv(pFormFillEnv), m_pWidget(pWidget) {
  ASSERT(m_pFormFillEnv);
}

CFFL_FormFiller::~CFFL_FormFiller() {
  DestroyWindows();
}

void CFFL_FormFiller::DestroyWindows() {
  while (!m_Maps.empty()) {
    auto it = m_Maps.begin();
    std::unique_ptr<CPWL_Wnd> pWnd = std::move(it->second);
    m_Maps.erase(it);
    pWnd->InvalidateProvider(this);
    pWnd->Destroy();
  }
}

FX_RECT CFFL_FormFiller::GetViewBBox(CPDFSDK_PageView* pPageView) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  CFX_FloatRect rcAnnot =
      pWnd ? PWLtoFFL(pWnd->GetWindowRect()) : m_pWidget->GetRect();
  CFX_FloatRect rcFocus = GetFocusBox(pPageView);

  CFX_FloatRect rcWin = rcAnnot;
  if (!rcFocus.IsEmpty())
    rcWin.Union(rcFocus);
  if (!rcWin.IsEmpty()) {
    rcWin.Inflate(1, 1);
    rcWin.Normalize();
  }

  return rcWin.GetOuterRect();
}

void CFFL_FormFiller::OnDraw(CPDFSDK_PageView* pPageView,
                             CPDFSDK_Annot* pAnnot,
                             CFX_RenderDevice* pDevice,
                             const CFX_Matrix& mtUser2Device) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (pWnd) {
    pWnd->DrawAppearance(pDevice, GetCurMatrix() * mtUser2Device);
    return;
  }

  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot);
  if (!CFFL_InteractiveFormFiller::IsVisible(pWidget))
    return;

  pWidget->DrawAppearance(pDevice, mtUser2Device, CPDF_Annot::Normal, nullptr);
}

void CFFL_FormFiller::OnDrawDeactive(CPDFSDK_PageView* pPageView,
                                     CPDFSDK_Annot* pAnnot,
                                     CFX_RenderDevice* pDevice,
                                     const CFX_Matrix& mtUser2Device) {
  ToCPDFSDKWidget(pAnnot)->DrawAppearance(pDevice, mtUser2Device,
                                          CPDF_Annot::Normal, nullptr);
}

void CFFL_FormFiller::OnMouseEnter(CPDFSDK_PageView* pPageView) {}

void CFFL_FormFiller::OnMouseExit(CPDFSDK_PageView* pPageView) {
  m_pTimer.reset();
  ASSERT(m_pWidget);
}

bool CFFL_FormFiller::OnLButtonDown(CPDFSDK_PageView* pPageView,
                                    CPDFSDK_Annot* pAnnot,
                                    uint32_t nFlags,
                                    const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, true);
  if (!pWnd)
    return false;

  m_bValid = true;
  FX_RECT rect = GetViewBBox(pPageView);
  InvalidateRect(rect);
  if (!rect.Contains(static_cast<int>(point.x), static_cast<int>(point.y)))
    return false;
  return pWnd->OnLButtonDown(WndtoPWL(pPageView, point), nFlags);
}

bool CFFL_FormFiller::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (!pWnd)
    return false;

  InvalidateRect(GetViewBBox(pPageView));
  pWnd->OnLButtonUp(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                                      uint32_t nFlags,
                                      const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnLButtonDblClk(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnMouseMove(CPDFSDK_PageView* pPageView,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnMouseMove(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnMouseWheel(CPDFSDK_PageView* pPageView,
                                   uint32_t nFlags,
                                   short zDelta,
                                   const CFX_PointF& point) {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, true);
  return pWnd && pWnd->OnMouseWheel(zDelta, WndtoPWL(pPageView, point), nFlags);
}

bool CFFL_FormFiller::OnRButtonDown(CPDFSDK_PageView* pPageView,
                                    uint32_t nFlags,
                                    const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, true);
  if (!pWnd)
    return false;

  pWnd->OnRButtonDown(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnRButtonUp(CPDFSDK_PageView* pPageView,
                                  uint32_t nFlags,
                                  const CFX_PointF& point) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (!pWnd)
    return false;

  pWnd->OnRButtonUp(WndtoPWL(pPageView, point), nFlags);
  return true;
}

bool CFFL_FormFiller::OnKeyDown(uint32_t nKeyCode, uint32_t nFlags) {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->OnKeyDown(nKeyCode, nFlags);
}

bool CFFL_FormFiller::OnChar(CPDFSDK_Annot* pAnnot,
                             uint32_t nChar,
                             uint32_t nFlags) {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->OnChar(nChar, nFlags);
}

bool CFFL_FormFiller::SetIndexSelected(int index, bool selected) {
  return false;
}

bool CFFL_FormFiller::IsIndexSelected(int index) {
  return false;
}

WideString CFFL_FormFiller::GetText() {
  if (!IsValid())
    return WideString();

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd ? pWnd->GetText() : WideString();
}

WideString CFFL_FormFiller::GetSelectedText() {
  if (!IsValid())
    return WideString();

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd ? pWnd->GetSelectedText() : WideString();
}

void CFFL_FormFiller::ReplaceSelection(const WideString& text) {
  if (!IsValid())
    return;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  if (!pWnd)
    return;

  pWnd->ReplaceSelection(text);
}

bool CFFL_FormFiller::CanUndo() {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->CanUndo();
}

bool CFFL_FormFiller::CanRedo() {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->CanRedo();
}

bool CFFL_FormFiller::Undo() {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->Undo();
}

bool CFFL_FormFiller::Redo() {
  if (!IsValid())
    return false;

  CPWL_Wnd* pWnd = GetPWLWindow(GetCurPageView(true), false);
  return pWnd && pWnd->Redo();
}

void CFFL_FormFiller::SetFocusForAnnot(CPDFSDK_Annot* pAnnot, uint32_t nFlag) {
  CPDFSDK_Widget* pWidget = ToCPDFSDKWidget(pAnnot);
  IPDF_Page* pPage = pWidget->GetPage();
  CPDFSDK_PageView* pPageView = m_pFormFillEnv->GetPageView(pPage, true);
  if (CPWL_Wnd* pWnd = GetPWLWindow(pPageView, true))
    pWnd->SetFocus();

  m_bValid = true;
  InvalidateRect(GetViewBBox(pPageView));
}

void CFFL_FormFiller::KillFocusForAnnot(uint32_t nFlag) {
  if (!IsValid())
    return;

  CPDFSDK_PageView* pPageView = GetCurPageView(false);
  if (!pPageView || !CommitData(pPageView, nFlag))
    return;
  if (CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false))
    pWnd->KillFocus();

  bool bDestroyPWLWindow;
  switch (m_pWidget->GetFieldType()) {
    case FormFieldType::kPushButton:
    case FormFieldType::kCheckBox:
    case FormFieldType::kRadioButton:
      bDestroyPWLWindow = true;
      break;
    default:
      bDestroyPWLWindow = false;
      break;
  }
  EscapeFiller(pPageView, bDestroyPWLWindow);
}

bool CFFL_FormFiller::IsValid() const {
  return m_bValid;
}

CPWL_Wnd::CreateParams CFFL_FormFiller::GetCreateParam() {
  CPWL_Wnd::CreateParams cp;
  cp.pProvider.Reset(this);
  cp.rcRectWnd = GetPDFAnnotRect();

  uint32_t dwCreateFlags = PWS_BORDER | PWS_BACKGROUND | PWS_VISIBLE;
  uint32_t dwFieldFlag = m_pWidget->GetFieldFlags();
  if (dwFieldFlag & pdfium::form_flags::kReadOnly)
    dwCreateFlags |= PWS_READONLY;

  Optional<FX_COLORREF> color = m_pWidget->GetFillColor();
  if (color.has_value())
    cp.sBackgroundColor = CFX_Color(color.value());
  color = m_pWidget->GetBorderColor();
  if (color.has_value())
    cp.sBorderColor = CFX_Color(color.value());

  cp.sTextColor = CFX_Color(CFX_Color::kGray, 0);

  color = m_pWidget->GetTextColor();
  if (color.has_value())
    cp.sTextColor = CFX_Color(color.value());

  cp.fFontSize = m_pWidget->GetFontSize();
  cp.dwBorderWidth = m_pWidget->GetBorderWidth();

  cp.nBorderStyle = m_pWidget->GetBorderStyle();
  switch (cp.nBorderStyle) {
    case BorderStyle::DASH:
      cp.sDash = CPWL_Dash(3, 3, 0);
      break;
    case BorderStyle::BEVELED:
    case BorderStyle::INSET:
      cp.dwBorderWidth *= 2;
      break;
    default:
      break;
  }

  if (cp.fFontSize <= 0)
    dwCreateFlags |= PWS_AUTOFONTSIZE;

  cp.dwFlags = dwCreateFlags;
  cp.pTimerHandler = m_pFormFillEnv->GetTimerHandler();
  cp.pSystemHandler = m_pFormFillEnv->GetSysHandler();
  return cp;
}

CPWL_Wnd* CFFL_FormFiller::GetPWLWindow(CPDFSDK_PageView* pPageView,
                                        bool bNew) {
  ASSERT(pPageView);
  auto it = m_Maps.find(pPageView);
  if (it == m_Maps.end()) {
    if (!bNew)
      return nullptr;

    CPWL_Wnd::CreateParams cp = GetCreateParam();
    auto pPrivateData = pdfium::MakeUnique<CFFL_PrivateData>();
    pPrivateData->pWidget.Reset(m_pWidget.Get());
    pPrivateData->pPageView = pPageView;
    pPrivateData->nWidgetAppearanceAge = m_pWidget->GetAppearanceAge();
    pPrivateData->nWidgetValueAge = 0;
    m_Maps[pPageView] = NewPWLWindow(cp, std::move(pPrivateData));
    return m_Maps[pPageView].get();
  }

  CPWL_Wnd* pWnd = it->second.get();
  if (!bNew)
    return pWnd;

  const auto* pPrivateData =
      static_cast<const CFFL_PrivateData*>(pWnd->GetAttachedData());
  if (pPrivateData->nWidgetAppearanceAge == m_pWidget->GetAppearanceAge())
    return pWnd;

  return ResetPWLWindow(
      pPageView, pPrivateData->nWidgetValueAge == m_pWidget->GetValueAge());
}

void CFFL_FormFiller::DestroyPWLWindow(CPDFSDK_PageView* pPageView) {
  auto it = m_Maps.find(pPageView);
  if (it == m_Maps.end())
    return;

  std::unique_ptr<CPWL_Wnd> pWnd = std::move(it->second);
  m_Maps.erase(it);
  pWnd->Destroy();
}

CFX_Matrix CFFL_FormFiller::GetWindowMatrix(
    const IPWL_SystemHandler::PerWindowData* pAttached) {
  const auto* pPrivateData = static_cast<const CFFL_PrivateData*>(pAttached);
  if (!pPrivateData || !pPrivateData->pPageView)
    return CFX_Matrix();

  return GetCurMatrix() * pPrivateData->pPageView->GetCurrentMatrix();
}

CFX_Matrix CFFL_FormFiller::GetCurMatrix() {
  CFX_Matrix mt;
  CFX_FloatRect rcDA = m_pWidget->GetPDFAnnot()->GetRect();
  switch (m_pWidget->GetRotate()) {
    case 90:
      mt = CFX_Matrix(0, 1, -1, 0, rcDA.right - rcDA.left, 0);
      break;
    case 180:
      mt = CFX_Matrix(-1, 0, 0, -1, rcDA.right - rcDA.left,
                      rcDA.top - rcDA.bottom);
      break;
    case 270:
      mt = CFX_Matrix(0, -1, 1, 0, 0, rcDA.top - rcDA.bottom);
      break;
    case 0:
    default:
      break;
  }
  mt.e += rcDA.left;
  mt.f += rcDA.bottom;

  return mt;
}

CFX_FloatRect CFFL_FormFiller::GetPDFAnnotRect() const {
  CFX_FloatRect rectAnnot = m_pWidget->GetPDFAnnot()->GetRect();

  float fWidth = rectAnnot.Width();
  float fHeight = rectAnnot.Height();
  if ((m_pWidget->GetRotate() / 90) & 0x01)
    std::swap(fWidth, fHeight);
  return CFX_FloatRect(0, 0, fWidth, fHeight);
}

CPDFSDK_PageView* CFFL_FormFiller::GetCurPageView(bool renew) {
  IPDF_Page* pPage = m_pWidget->GetPage();
  return m_pFormFillEnv->GetPageView(pPage, renew);
}

CFX_FloatRect CFFL_FormFiller::GetFocusBox(CPDFSDK_PageView* pPageView) {
  CPWL_Wnd* pWnd = GetPWLWindow(pPageView, false);
  if (!pWnd)
    return CFX_FloatRect();

  CFX_FloatRect rcFocus = FFLtoWnd(pPageView, PWLtoFFL(pWnd->GetFocusRect()));
  return pPageView->GetPDFPage()->GetBBox().Contains(rcFocus) ? rcFocus
                                                              : CFX_FloatRect();
}

CFX_FloatRect CFFL_FormFiller::FFLtoPWL(const CFX_FloatRect& rect) {
  return GetCurMatrix().GetInverse().TransformRect(rect);
}

CFX_FloatRect CFFL_FormFiller::PWLtoFFL(const CFX_FloatRect& rect) {
  return GetCurMatrix().TransformRect(rect);
}

CFX_PointF CFFL_FormFiller::FFLtoPWL(const CFX_PointF& point) {
  return GetCurMatrix().GetInverse().Transform(point);
}

CFX_PointF CFFL_FormFiller::PWLtoFFL(const CFX_PointF& point) {
  return GetCurMatrix().Transform(point);
}

CFX_PointF CFFL_FormFiller::WndtoPWL(CPDFSDK_PageView* pPageView,
                                     const CFX_PointF& pt) {
  return FFLtoPWL(pt);
}

CFX_FloatRect CFFL_FormFiller::FFLtoWnd(CPDFSDK_PageView* pPageView,
                                        const CFX_FloatRect& rect) {
  return rect;
}

bool CFFL_FormFiller::CommitData(CPDFSDK_PageView* pPageView, uint32_t nFlag) {
  if (!IsDataChanged(pPageView))
    return true;

  CFFL_InteractiveFormFiller* pFormFiller =
      m_pFormFillEnv->GetInteractiveFormFiller();
  ObservedPtr<CPDFSDK_Annot> pObserved(m_pWidget.Get());

  if (!pFormFiller->OnKeyStrokeCommit(&pObserved, pPageView, nFlag)) {
    if (!pObserved)
      return false;
    ResetPWLWindow(pPageView, false);
    return true;
  }
  if (!pObserved)
    return false;

  if (!pFormFiller->OnValidate(&pObserved, pPageView, nFlag)) {
    if (!pObserved)
      return false;
    ResetPWLWindow(pPageView, false);
    return true;
  }
  if (!pObserved)
    return false;

  SaveData(pPageView);  // may invoking JS to delete this widget.
  if (!pObserved)
    return false;

  pFormFiller->OnCalculate(&pObserved, pPageView, nFlag);
  if (!pObserved)
    return false;

  pFormFiller->OnFormat(&pObserved, pPageView, nFlag);
  if (!pObserved)
    return false;

  return true;
}

bool CFFL_FormFiller::IsDataChanged(CPDFSDK_PageView* pPageView) {
  return false;
}

void CFFL_FormFiller::SaveData(CPDFSDK_PageView* pPageView) {}

#ifdef PDF_ENABLE_XFA
bool CFFL_FormFiller::IsFieldFull(CPDFSDK_PageView* pPageView) {
  return false;
}
#endif  // PDF_ENABLE_XFA

void CFFL_FormFiller::SetChangeMark() {
  m_pFormFillEnv->OnChange();
}

void CFFL_FormFiller::GetActionData(CPDFSDK_PageView* pPageView,
                                    CPDF_AAction::AActionType type,
                                    CPDFSDK_FieldAction& fa) {
  fa.sValue = m_pWidget->GetValue();
}

void CFFL_FormFiller::SetActionData(CPDFSDK_PageView* pPageView,
                                    CPDF_AAction::AActionType type,
                                    const CPDFSDK_FieldAction& fa) {}

bool CFFL_FormFiller::IsActionDataChanged(CPDF_AAction::AActionType type,
                                          const CPDFSDK_FieldAction& faOld,
                                          const CPDFSDK_FieldAction& faNew) {
  return false;
}

void CFFL_FormFiller::SaveState(CPDFSDK_PageView* pPageView) {}

void CFFL_FormFiller::RestoreState(CPDFSDK_PageView* pPageView) {}

CPWL_Wnd* CFFL_FormFiller::ResetPWLWindow(CPDFSDK_PageView* pPageView,
                                          bool bRestoreValue) {
  return GetPWLWindow(pPageView, false);
}

void CFFL_FormFiller::OnTimerFired() {}

IPWL_SystemHandler* CFFL_FormFiller::GetSystemHandler() const {
  return m_pFormFillEnv->GetSysHandler();
}

void CFFL_FormFiller::EscapeFiller(CPDFSDK_PageView* pPageView,
                                   bool bDestroyPWLWindow) {
  m_bValid = false;

  InvalidateRect(GetViewBBox(pPageView));
  if (bDestroyPWLWindow)
    DestroyPWLWindow(pPageView);
}

void CFFL_FormFiller::InvalidateRect(const FX_RECT& rect) {
  m_pFormFillEnv->Invalidate(m_pWidget->GetPage(), rect);
}
