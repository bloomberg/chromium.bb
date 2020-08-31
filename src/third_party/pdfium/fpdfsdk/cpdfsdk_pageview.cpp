// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_pageview.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fpdfdoc/cpdf_interactiveform.h"
#include "core/fxcrt/autorestorer.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_annotiteration.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_helpers.h"
#include "fpdfsdk/cpdfsdk_interactiveform.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_page.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_widget.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#endif  // PDF_ENABLE_XFA

CPDFSDK_PageView::CPDFSDK_PageView(CPDFSDK_FormFillEnvironment* pFormFillEnv,
                                   IPDF_Page* page)
    : m_page(page), m_pFormFillEnv(pFormFillEnv) {
  ASSERT(m_page);
  CPDF_Page* pPDFPage = ToPDFPage(page);
  if (pPDFPage) {
    CPDFSDK_InteractiveForm* pForm = pFormFillEnv->GetInteractiveForm();
    CPDF_InteractiveForm* pPDFForm = pForm->GetInteractiveForm();
    pPDFForm->FixPageFields(pPDFPage);
    if (!page->AsXFAPage())
      pPDFPage->SetView(this);
  }
}

CPDFSDK_PageView::~CPDFSDK_PageView() {
  if (!m_page->AsXFAPage()) {
    // The call to |ReleaseAnnot| can cause the page pointed to by |m_page| to
    // be freed, which will cause issues if we try to cleanup the pageview
    // pointer in |m_page|. So, reset the pageview pointer before doing anything
    // else.
    m_page->AsPDFPage()->SetView(nullptr);
  }

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray)
    pAnnotHandlerMgr->ReleaseAnnot(pdfium::WrapUnique(pAnnot));

  m_SDKAnnotArray.clear();
  m_pAnnotList.reset();
}

void CPDFSDK_PageView::PageView_OnDraw(CFX_RenderDevice* pDevice,
                                       const CFX_Matrix& mtUser2Device,
                                       CPDF_RenderOptions* pOptions,
                                       const FX_RECT& pClip) {
  m_curMatrix = mtUser2Device;

#ifdef PDF_ENABLE_XFA
  IPDF_Page* pPage = GetXFAPage();
  CPDF_Document::Extension* pContext =
      pPage ? pPage->GetDocument()->GetExtension() : nullptr;
  if (pContext && pContext->ContainsExtensionFullForm()) {
    static_cast<CPDFXFA_Page*>(pPage)->DrawFocusAnnot(pDevice, GetFocusAnnot(),
                                                      mtUser2Device, pClip);
    return;
  }
#endif  // PDF_ENABLE_XFA

  // for pdf/static xfa.
  CPDFSDK_AnnotIteration annotIteration(this, true);
  for (const auto& pSDKAnnot : annotIteration) {
    m_pFormFillEnv->GetAnnotHandlerMgr()->Annot_OnDraw(
        this, pSDKAnnot.Get(), pDevice, mtUser2Device,
        pOptions->GetDrawAnnots());
  }
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFXAnnotAtPoint(const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotMgr = m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_AnnotIteration annotIteration(this, false);
  for (const auto& pSDKAnnot : annotIteration) {
    CFX_FloatRect rc = pAnnotMgr->Annot_OnGetViewBBox(this, pSDKAnnot.Get());
    if (pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::POPUP)
      continue;
    if (rc.Contains(point))
      return pSDKAnnot.Get();
  }
  return nullptr;
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFXWidgetAtPoint(const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotMgr = m_pFormFillEnv->GetAnnotHandlerMgr();
  CPDFSDK_AnnotIteration annotIteration(this, false);
  for (const auto& pSDKAnnot : annotIteration) {
    bool bHitTest = pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::WIDGET;
#ifdef PDF_ENABLE_XFA
    bHitTest = bHitTest ||
               pSDKAnnot->GetAnnotSubtype() == CPDF_Annot::Subtype::XFAWIDGET;
#endif  // PDF_ENABLE_XFA
    if (bHitTest) {
      pAnnotMgr->Annot_OnGetViewBBox(this, pSDKAnnot.Get());
      if (pAnnotMgr->Annot_OnHitTest(this, pSDKAnnot.Get(), point))
        return pSDKAnnot.Get();
    }
  }
  return nullptr;
}

#ifdef PDF_ENABLE_XFA
CPDFSDK_Annot* CPDFSDK_PageView::AddAnnot(CXFA_FFWidget* pPDFAnnot) {
  CPDFSDK_Annot* pSDKAnnot = GetAnnotByXFAWidget(pPDFAnnot);
  if (pSDKAnnot)
    return pSDKAnnot;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandler = m_pFormFillEnv->GetAnnotHandlerMgr();
  std::unique_ptr<CPDFSDK_Annot> pNewAnnot =
      pAnnotHandler->NewXFAAnnot(pPDFAnnot, this);
  ASSERT(pNewAnnot);
  pSDKAnnot = pNewAnnot.get();
  // TODO(thestig): See if |m_SDKAnnotArray|, which takes ownership of
  // |pNewAnnot|, can hold std::unique_ptrs instead of raw pointers.
  m_SDKAnnotArray.push_back(pNewAnnot.release());
  return pSDKAnnot;
}

bool CPDFSDK_PageView::DeleteAnnot(CPDFSDK_Annot* pAnnot) {
  IPDF_Page* pPage = pAnnot->GetXFAPage();
  if (!pPage)
    return false;

  CPDF_Document::Extension* pContext = pPage->GetDocument()->GetExtension();
  if (pContext && !pContext->ContainsExtensionForm())
    return false;

  ObservedPtr<CPDFSDK_Annot> pObserved(pAnnot);
  if (GetFocusAnnot() == pAnnot)
    m_pFormFillEnv->KillFocusAnnot(0);  // May invoke JS, invalidating pAnnot.

  if (pObserved) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandler =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    if (pAnnotHandler)
      pAnnotHandler->ReleaseAnnot(pdfium::WrapUnique(pObserved.Get()));
  }

  auto it = std::find(m_SDKAnnotArray.begin(), m_SDKAnnotArray.end(), pAnnot);
  if (it != m_SDKAnnotArray.end())
    m_SDKAnnotArray.erase(it);
  if (m_pCaptureWidget.Get() == pAnnot)
    m_pCaptureWidget.Reset();

  return true;
}
#endif  // PDF_ENABLE_XFA

CPDF_Document* CPDFSDK_PageView::GetPDFDocument() {
  return m_page->GetDocument();
}

CPDF_Page* CPDFSDK_PageView::GetPDFPage() const {
  return ToPDFPage(m_page);
}

CPDFSDK_Annot* CPDFSDK_PageView::GetAnnotByDict(CPDF_Dictionary* pDict) {
  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray) {
    if (pAnnot->GetPDFAnnot()->GetAnnotDict() == pDict)
      return pAnnot;
  }
  return nullptr;
}

#ifdef PDF_ENABLE_XFA
CPDFSDK_Annot* CPDFSDK_PageView::GetAnnotByXFAWidget(CXFA_FFWidget* pWidget) {
  if (!pWidget)
    return nullptr;

  for (CPDFSDK_Annot* pAnnot : m_SDKAnnotArray) {
    CPDFXFA_Widget* pCurrentWidget = ToXFAWidget(pAnnot);
    if (pCurrentWidget && pCurrentWidget->GetXFAFFWidget() == pWidget)
      return pAnnot;
  }
  return nullptr;
}

IPDF_Page* CPDFSDK_PageView::GetXFAPage() {
  return ToXFAPage(m_page);
}
#endif  // PDF_ENABLE_XFA

WideString CPDFSDK_PageView::GetFocusedFormText() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_GetText(pAnnot);
  }

  return WideString();
}

WideString CPDFSDK_PageView::GetSelectedText() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_GetSelectedText(pAnnot);
  }

  return WideString();
}

void CPDFSDK_PageView::ReplaceSelection(const WideString& text) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    pAnnotHandlerMgr->Annot_ReplaceSelection(pAnnot, text);
  }
}

bool CPDFSDK_PageView::CanUndo() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_CanUndo(pAnnot);
  }
  return false;
}

bool CPDFSDK_PageView::CanRedo() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_CanRedo(pAnnot);
  }
  return false;
}

bool CPDFSDK_PageView::Undo() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_Undo(pAnnot);
  }
  return false;
}

bool CPDFSDK_PageView::Redo() {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_Redo(pAnnot);
  }
  return false;
}

bool CPDFSDK_PageView::OnFocus(uint32_t nFlag, const CFX_PointF& point) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot) {
    m_pFormFillEnv->KillFocusAnnot(nFlag);
    return false;
  }

  m_pFormFillEnv->SetFocusAnnot(&pAnnot);
  return true;
}

bool CPDFSDK_PageView::OnLButtonDown(uint32_t nFlag, const CFX_PointF& point) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot) {
    m_pFormFillEnv->KillFocusAnnot(nFlag);
    return false;
  }

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  if (!pAnnotHandlerMgr->Annot_OnLButtonDown(this, &pAnnot, nFlag, point))
    return false;

  if (!pAnnot)
    return false;

  m_pFormFillEnv->SetFocusAnnot(&pAnnot);
  return true;
}

bool CPDFSDK_PageView::OnLButtonUp(uint32_t nFlag, const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  ObservedPtr<CPDFSDK_Annot> pFXAnnot(GetFXWidgetAtPoint(point));
  ObservedPtr<CPDFSDK_Annot> pFocusAnnot(GetFocusAnnot());
  if (pFocusAnnot && pFocusAnnot != pFXAnnot) {
    // Last focus Annot gets a chance to handle the event.
    if (pAnnotHandlerMgr->Annot_OnLButtonUp(this, &pFocusAnnot, nFlag, point))
      return true;
  }
  return pFXAnnot &&
         pAnnotHandlerMgr->Annot_OnLButtonUp(this, &pFXAnnot, nFlag, point);
}

bool CPDFSDK_PageView::OnLButtonDblClk(uint32_t nFlag,
                                       const CFX_PointF& point) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot) {
    m_pFormFillEnv->KillFocusAnnot(nFlag);
    return false;
  }

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  if (!pAnnotHandlerMgr->Annot_OnLButtonDblClk(this, &pAnnot, nFlag, point))
    return false;

  if (!pAnnot)
    return false;

  m_pFormFillEnv->SetFocusAnnot(&pAnnot);
  return true;
}

bool CPDFSDK_PageView::OnRButtonDown(uint32_t nFlag, const CFX_PointF& point) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot)
    return false;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  bool ok = pAnnotHandlerMgr->Annot_OnRButtonDown(this, &pAnnot, nFlag, point);
  if (!pAnnot)
    return false;

  if (ok)
    m_pFormFillEnv->SetFocusAnnot(&pAnnot);

  return true;
}

bool CPDFSDK_PageView::OnRButtonUp(uint32_t nFlag, const CFX_PointF& point) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot)
    return false;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  bool ok = pAnnotHandlerMgr->Annot_OnRButtonUp(this, &pAnnot, nFlag, point);
  if (!pAnnot)
    return false;

  if (ok)
    m_pFormFillEnv->SetFocusAnnot(&pAnnot);

  return true;
}

bool CPDFSDK_PageView::OnMouseMove(int nFlag, const CFX_PointF& point) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();

  ObservedPtr<CPDFSDK_Annot> pFXAnnot(GetFXAnnotAtPoint(point));
  ObservedPtr<CPDFSDK_PageView> pThis(this);

  if (m_bOnWidget && m_pCaptureWidget != pFXAnnot)
    ExitWidget(pAnnotHandlerMgr, true, nFlag);

  // ExitWidget() may have invalidated objects.
  if (!pThis || !pFXAnnot)
    return false;

  if (!m_bOnWidget) {
    EnterWidget(pAnnotHandlerMgr, &pFXAnnot, nFlag);

    // EnterWidget() may have invalidated objects.
    if (!pThis)
      return false;

    if (!pFXAnnot) {
      ExitWidget(pAnnotHandlerMgr, false, nFlag);
      return true;
    }
  }
  pAnnotHandlerMgr->Annot_OnMouseMove(this, &pFXAnnot, nFlag, point);
  return true;
}

void CPDFSDK_PageView::EnterWidget(CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr,
                                   ObservedPtr<CPDFSDK_Annot>* pAnnot,
                                   uint32_t nFlag) {
  m_bOnWidget = true;
  m_pCaptureWidget.Reset(pAnnot->Get());
  pAnnotHandlerMgr->Annot_OnMouseEnter(this, pAnnot, nFlag);
}

void CPDFSDK_PageView::ExitWidget(CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr,
                                  bool callExitCallback,
                                  uint32_t nFlag) {
  m_bOnWidget = false;
  if (!m_pCaptureWidget)
    return;

  if (callExitCallback) {
    ObservedPtr<CPDFSDK_PageView> pThis(this);
    pAnnotHandlerMgr->Annot_OnMouseExit(this, &m_pCaptureWidget, nFlag);

    // Annot_OnMouseExit() may have invalidated |this|.
    if (!pThis)
      return;
  }

  m_pCaptureWidget.Reset();
}

bool CPDFSDK_PageView::OnMouseWheel(int nFlag,
                                    const CFX_PointF& point,
                                    const CFX_Vector& delta) {
  ObservedPtr<CPDFSDK_Annot> pAnnot(GetFXWidgetAtPoint(point));
  if (!pAnnot)
    return false;

  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  return pAnnotHandlerMgr->Annot_OnMouseWheel(this, &pAnnot, nFlag, point,
                                              delta);
}

bool CPDFSDK_PageView::SetIndexSelected(int index, bool selected) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    ObservedPtr<CPDFSDK_Annot> pAnnotObserved(pAnnot);
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_SetIndexSelected(&pAnnotObserved, index,
                                                    selected);
  }

  return false;
}

bool CPDFSDK_PageView::IsIndexSelected(int index) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    ObservedPtr<CPDFSDK_Annot> pAnnotObserved(pAnnot);
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_IsIndexSelected(&pAnnotObserved, index);
  }

  return false;
}

bool CPDFSDK_PageView::OnChar(int nChar, uint32_t nFlag) {
  if (CPDFSDK_Annot* pAnnot = GetFocusAnnot()) {
    CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
        m_pFormFillEnv->GetAnnotHandlerMgr();
    return pAnnotHandlerMgr->Annot_OnChar(pAnnot, nChar, nFlag);
  }

  return false;
}

bool CPDFSDK_PageView::OnKeyDown(int nKeyCode, int nFlag) {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();
  return pAnnotHandlerMgr->Annot_OnKeyDown(this, GetFocusAnnot(), nKeyCode,
                                           nFlag);
}

bool CPDFSDK_PageView::OnKeyUp(int nKeyCode, int nFlag) {
  return false;
}

void CPDFSDK_PageView::LoadFXAnnots() {
  CPDFSDK_AnnotHandlerMgr* pAnnotHandlerMgr =
      m_pFormFillEnv->GetAnnotHandlerMgr();

  AutoRestorer<bool> lock(&m_bLocked);
  m_bLocked = true;

#ifdef PDF_ENABLE_XFA
  RetainPtr<CPDFXFA_Page> protector(ToXFAPage(m_page));
  CPDF_Document::Extension* pContext = m_pFormFillEnv->GetDocExtension();
  if (pContext && pContext->ContainsExtensionFullForm()) {
    CXFA_FFPageView* pageView = protector->GetXFAPageView();
    std::unique_ptr<IXFA_WidgetIterator> pWidgetHandler =
        pageView->CreateFormWidgetIterator(XFA_WidgetStatus_Visible |
                                           XFA_WidgetStatus_Viewable);

    while (CXFA_FFWidget* pXFAAnnot = pWidgetHandler->MoveToNext()) {
      std::unique_ptr<CPDFSDK_Annot> pNewAnnot =
          pAnnotHandlerMgr->NewXFAAnnot(pXFAAnnot, this);
      ASSERT(pNewAnnot);
      CPDFSDK_Annot* pAnnot = pNewAnnot.get();
      m_SDKAnnotArray.push_back(pNewAnnot.release());
      pAnnotHandlerMgr->Annot_OnLoad(pAnnot);
    }

    return;
  }
#endif  // PDF_ENABLE_XFA

  CPDF_Page* pPage = GetPDFPage();
  ASSERT(pPage);
  bool bUpdateAP = CPDF_InteractiveForm::IsUpdateAPEnabled();
  // Disable the default AP construction.
  CPDF_InteractiveForm::SetUpdateAP(false);
  m_pAnnotList = pdfium::MakeUnique<CPDF_AnnotList>(pPage);
  CPDF_InteractiveForm::SetUpdateAP(bUpdateAP);

  const size_t nCount = m_pAnnotList->Count();
  for (size_t i = 0; i < nCount; ++i) {
    CPDF_Annot* pPDFAnnot = m_pAnnotList->GetAt(i);
    CheckForUnsupportedAnnot(pPDFAnnot);
    std::unique_ptr<CPDFSDK_Annot> pAnnot =
        pAnnotHandlerMgr->NewAnnot(pPDFAnnot, this);
    if (!pAnnot)
      continue;
    m_SDKAnnotArray.push_back(pAnnot.release());
    pAnnotHandlerMgr->Annot_OnLoad(m_SDKAnnotArray.back());
  }
}

void CPDFSDK_PageView::UpdateRects(const std::vector<CFX_FloatRect>& rects) {
  for (const auto& rc : rects)
    m_pFormFillEnv->Invalidate(m_page, rc.GetOuterRect());
}

void CPDFSDK_PageView::UpdateView(CPDFSDK_Annot* pAnnot) {
  CFX_FloatRect rcWindow = pAnnot->GetRect();
  m_pFormFillEnv->Invalidate(m_page, rcWindow.GetOuterRect());
}

int CPDFSDK_PageView::GetPageIndex() const {
#ifdef PDF_ENABLE_XFA
  CPDF_Document::Extension* pContext = m_page->GetDocument()->GetExtension();
  if (pContext && pContext->ContainsExtensionFullForm()) {
    CXFA_FFPageView* pPageView = m_page->AsXFAPage()->GetXFAPageView();
    return pPageView ? pPageView->GetLayoutItem()->GetPageIndex() : -1;
  }
#endif  // PDF_ENABLE_XFA
  return GetPageIndexForStaticPDF();
}

bool CPDFSDK_PageView::IsValidAnnot(const CPDF_Annot* p) const {
  if (!p)
    return false;

  const auto& annots = m_pAnnotList->All();
  auto it = std::find_if(annots.begin(), annots.end(),
                         [p](const std::unique_ptr<CPDF_Annot>& annot) {
                           return annot.get() == p;
                         });
  return it != annots.end();
}

bool CPDFSDK_PageView::IsValidSDKAnnot(const CPDFSDK_Annot* p) const {
  if (!p)
    return false;
  return pdfium::ContainsValue(m_SDKAnnotArray, p);
}

CPDFSDK_Annot* CPDFSDK_PageView::GetFocusAnnot() {
  CPDFSDK_Annot* pFocusAnnot = m_pFormFillEnv->GetFocusAnnot();
  return IsValidSDKAnnot(pFocusAnnot) ? pFocusAnnot : nullptr;
}

int CPDFSDK_PageView::GetPageIndexForStaticPDF() const {
  const CPDF_Dictionary* pDict = GetPDFPage()->GetDict();
  CPDF_Document* pDoc = m_pFormFillEnv->GetPDFDocument();
  return pDoc->GetPageIndex(pDict->GetObjNum());
}
