// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cpdfxfa_widgethandler.h"

#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interactiveform.h"
#include "fpdfsdk/cpdfsdk_pageview.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "fpdfsdk/fpdfxfa/cpdfxfa_widget.h"
#include "third_party/base/check.h"
#include "xfa/fgas/graphics/cfgas_gegraphics.h"
#include "xfa/fwl/cfwl_app.h"
#include "xfa/fwl/fwl_widgetdef.h"
#include "xfa/fwl/fwl_widgethit.h"
#include "xfa/fxfa/cxfa_ffdocview.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"
#include "xfa/fxfa/cxfa_ffwidgethandler.h"
#include "xfa/fxfa/fxfa_basic.h"
#include "xfa/fxfa/parser/cxfa_node.h"

#define CHECK_FWL_VKEY_ENUM____(name)                                   \
  static_assert(static_cast<int>(name) == static_cast<int>(XFA_##name), \
                "FWL_VKEYCODE enum mismatch")

CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Back);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Tab);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NewLine);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Clear);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Return);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Shift);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Control);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Menu);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Pause);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Capital);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Kana);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Hangul);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Junja);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Final);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Hanja);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Kanji);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Escape);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Convert);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NonConvert);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Accept);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_ModeChange);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Space);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Prior);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Next);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_End);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Home);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Left);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Up);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Right);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Down);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Select);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Print);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Execute);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Snapshot);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Insert);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Delete);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Help);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_0);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_2);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_3);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_4);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_5);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_6);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_7);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_8);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_9);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_A);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_B);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_C);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_D);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_E);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_G);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_H);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_I);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_J);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_K);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_L);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_M);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_N);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_O);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_P);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Q);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_R);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_S);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_T);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_U);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_V);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_W);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_X);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Y);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Z);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_LWin);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Command);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_RWin);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Apps);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Sleep);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad0);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad2);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad3);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad4);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad5);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad6);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad7);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad8);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NumPad9);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Multiply);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Add);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Separator);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Subtract);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Decimal);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Divide);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F2);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F3);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F4);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F5);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F6);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F7);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F8);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F9);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F10);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F11);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F12);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F13);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F14);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F15);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F16);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F17);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F18);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F19);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F20);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F21);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F22);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F23);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_F24);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NunLock);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Scroll);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_LShift);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_RShift);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_LControl);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_RControl);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_LMenu);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_RMenu);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Back);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Forward);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Refresh);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Stop);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Search);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Favorites);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_BROWSER_Home);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_VOLUME_Mute);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_VOLUME_Down);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_VOLUME_Up);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_NEXT_Track);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_PREV_Track);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_Stop);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_PLAY_Pause);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_LAUNCH_Mail);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_LAUNCH_MEDIA_Select);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_LAUNCH_APP1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_MEDIA_LAUNCH_APP2);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_Plus);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_Comma);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_Minus);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_Period);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_2);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_3);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_4);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_5);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_6);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_7);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_8);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_102);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_ProcessKey);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Packet);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Attn);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Crsel);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Exsel);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Ereof);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Play);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Zoom);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_NoName);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_PA1);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_OEM_Clear);
CHECK_FWL_VKEY_ENUM____(FWL_VKEY_Unknown);

#undef CHECK_FWL_VKEY_ENUM____

CPDFXFA_WidgetHandler::CPDFXFA_WidgetHandler() = default;

CPDFXFA_WidgetHandler::~CPDFXFA_WidgetHandler() = default;

bool CPDFXFA_WidgetHandler::CanAnswer(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pWidget = ToXFAWidget(pAnnot);
  return pWidget && pWidget->GetXFAFFWidget();
}

std::unique_ptr<CPDFSDK_Annot> CPDFXFA_WidgetHandler::NewAnnot(
    CPDF_Annot* pAnnot,
    CPDFSDK_PageView* pPageView) {
  return nullptr;
}

std::unique_ptr<CPDFSDK_Annot> CPDFXFA_WidgetHandler::NewAnnotForXFA(
    CXFA_FFWidget* pFFWidget,
    CPDFSDK_PageView* pPageView) {
  CHECK(pPageView);
  return std::make_unique<CPDFXFA_Widget>(pFFWidget, pPageView);
}

void CPDFXFA_WidgetHandler::OnDraw(CPDFSDK_Annot* pAnnot,
                                   CFX_RenderDevice* pDevice,
                                   const CFX_Matrix& mtUser2Device,
                                   bool bDrawAnnots) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  bool bIsHighlight = false;
  if (GetFormFillEnvironment()->GetFocusAnnot() != pAnnot)
    bIsHighlight = true;

  CFGAS_GEGraphics gs(pDevice);
  GetXFAFFWidgetHandler()->RenderWidget(pXFAWidget->GetXFAFFWidget(), &gs,
                                        mtUser2Device, bIsHighlight);

  // to do highlight and shadow
}

void CPDFXFA_WidgetHandler::OnLoad(CPDFSDK_Annot* pAnnot) {}

void CPDFXFA_WidgetHandler::ReleaseAnnot(
    std::unique_ptr<CPDFSDK_Annot> pAnnot) {}

CFX_FloatRect CPDFXFA_WidgetHandler::GetViewBBox(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_Node* node = pXFAWidget->GetXFAFFWidget()->GetNode();
  DCHECK(node->IsWidgetReady());

  CFX_RectF rcBBox = pXFAWidget->GetXFAFFWidget()->GetBBox(
      node->GetFFWidgetType() == XFA_FFWidgetType::kSignature
          ? CXFA_FFWidget::kDrawFocus
          : CXFA_FFWidget::kDoNotDrawFocus);

  CFX_FloatRect rcWidget = rcBBox.ToFloatRect();
  rcWidget.Inflate(1.0f, 1.0f);
  return rcWidget;
}

WideString CPDFXFA_WidgetHandler::GetText(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->GetText(pXFAWidget->GetXFAFFWidget());
}

WideString CPDFXFA_WidgetHandler::GetSelectedText(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->GetSelectedText(pXFAWidget->GetXFAFFWidget());
}

void CPDFXFA_WidgetHandler::ReplaceSelection(CPDFSDK_Annot* pAnnot,
                                             const WideString& text) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->PasteText(pXFAWidget->GetXFAFFWidget(), text);
}

bool CPDFXFA_WidgetHandler::SelectAllText(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->SelectAllText(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::CanUndo(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->CanUndo(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::CanRedo(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->CanRedo(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::Undo(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->Undo(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::Redo(CPDFSDK_Annot* pAnnot) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->Redo(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::HitTest(CPDFSDK_Annot* pAnnot,
                                    const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  auto* pContext = static_cast<CPDFXFA_Context*>(
      GetFormFillEnvironment()->GetDocExtension());
  if (!pContext)
    return false;

  CXFA_FFDocView* pDocView = pContext->GetXFADocView();
  if (!pDocView)
    return false;

  CXFA_FFWidgetHandler* pWidgetHandler = pDocView->GetWidgetHandler();
  return pWidgetHandler &&
         pWidgetHandler->HitTest(pXFAWidget->GetXFAFFWidget(), point) !=
             FWL_WidgetHit::Unknown;
}

void CPDFXFA_WidgetHandler::OnMouseEnter(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlag) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  pWidgetHandler->OnMouseEnter(pXFAWidget->GetXFAFFWidget());
}

void CPDFXFA_WidgetHandler::OnMouseExit(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlag) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  pWidgetHandler->OnMouseExit(pXFAWidget->GetXFAFFWidget());
}

bool CPDFXFA_WidgetHandler::OnLButtonDown(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                          Mask<FWL_EVENTFLAG> nFlags,
                                          const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnLButtonDown(pXFAWidget->GetXFAFFWidget(),
                                       GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnLButtonUp(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlags,
                                        const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnLButtonUp(pXFAWidget->GetXFAFFWidget(),
                                     GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnLButtonDblClk(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                            Mask<FWL_EVENTFLAG> nFlags,
                                            const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnLButtonDblClk(pXFAWidget->GetXFAFFWidget(),
                                         GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnMouseMove(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlags,
                                        const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnMouseMove(pXFAWidget->GetXFAFFWidget(),
                                     GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnMouseWheel(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                         Mask<FWL_EVENTFLAG> nFlags,
                                         const CFX_PointF& point,
                                         const CFX_Vector& delta) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnMouseWheel(pXFAWidget->GetXFAFFWidget(),
                                      GetKeyFlags(nFlags), point, delta);
}

bool CPDFXFA_WidgetHandler::OnRButtonDown(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                          Mask<FWL_EVENTFLAG> nFlags,
                                          const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnRButtonDown(pXFAWidget->GetXFAFFWidget(),
                                       GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnRButtonUp(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlags,
                                        const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnRButtonUp(pXFAWidget->GetXFAFFWidget(),
                                     GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnRButtonDblClk(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                            Mask<FWL_EVENTFLAG> nFlags,
                                            const CFX_PointF& point) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnRButtonDblClk(pXFAWidget->GetXFAFFWidget(),
                                         GetKeyFlags(nFlags), point);
}

bool CPDFXFA_WidgetHandler::OnChar(CPDFSDK_Annot* pAnnot,
                                   uint32_t nChar,
                                   Mask<FWL_EVENTFLAG> nFlags) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnChar(pXFAWidget->GetXFAFFWidget(), nChar,
                                GetKeyFlags(nFlags));
}

bool CPDFXFA_WidgetHandler::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                      FWL_VKEYCODE nKeyCode,
                                      Mask<FWL_EVENTFLAG> nFlag) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnKeyDown(pXFAWidget->GetXFAFFWidget(),
                                   static_cast<XFA_FWL_VKEYCODE>(nKeyCode),
                                   GetKeyFlags(nFlag));
}

bool CPDFXFA_WidgetHandler::OnKeyUp(CPDFSDK_Annot* pAnnot,
                                    FWL_VKEYCODE nKeyCode,
                                    Mask<FWL_EVENTFLAG> nFlag) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot);
  CXFA_FFWidgetHandler* pWidgetHandler = GetXFAFFWidgetHandler();
  return pWidgetHandler->OnKeyUp(pXFAWidget->GetXFAFFWidget(),
                                 static_cast<XFA_FWL_VKEYCODE>(nKeyCode),
                                 GetKeyFlags(nFlag));
}

bool CPDFXFA_WidgetHandler::OnSetFocus(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                       Mask<FWL_EVENTFLAG> nFlag) {
  return true;
}

bool CPDFXFA_WidgetHandler::OnKillFocus(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                        Mask<FWL_EVENTFLAG> nFlag) {
  CPDFXFA_Widget* pXFAWidget = ToXFAWidget(pAnnot.Get());
  CXFA_FFWidget* hWidget = pXFAWidget->GetXFAFFWidget();
  if (!hWidget)
    return true;

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView)
    return true;

  pXFAPageView->GetDocView()->SetFocus(nullptr);
  return true;
}

bool CPDFXFA_WidgetHandler::OnXFAChangedFocus(
    ObservedPtr<CPDFSDK_Annot>& pNewAnnot) {
  if (!pNewAnnot || !GetXFAFFWidgetHandler())
    return true;

  CPDFXFA_Widget* pNewXFAWidget = ToXFAWidget(pNewAnnot.Get());
  if (!pNewXFAWidget)
    return true;

  CXFA_FFWidget* hWidget = pNewXFAWidget->GetXFAFFWidget();
  if (!hWidget)
    return true;

  CXFA_FFPageView* pXFAPageView = hWidget->GetPageView();
  if (!pXFAPageView)
    return true;

  if (pXFAPageView->GetDocView()->SetFocus(hWidget))
    return true;

  return pXFAPageView->GetDocView()->GetFocusWidget() == hWidget;
}

bool CPDFXFA_WidgetHandler::SetIndexSelected(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                             int index,
                                             bool selected) {
  return false;
}

bool CPDFXFA_WidgetHandler::IsIndexSelected(ObservedPtr<CPDFSDK_Annot>& pAnnot,
                                            int index) {
  return false;
}

CXFA_FFWidgetHandler* CPDFXFA_WidgetHandler::GetXFAFFWidgetHandler() {
  auto* pDoc = static_cast<CPDFXFA_Context*>(
      GetFormFillEnvironment()->GetDocExtension());
  if (!pDoc)
    return nullptr;

  CXFA_FFDocView* pDocView = pDoc->GetXFADocView();
  if (!pDocView)
    return nullptr;

  return pDocView->GetWidgetHandler();
}

Mask<XFA_FWL_KeyFlag> CPDFXFA_WidgetHandler::GetKeyFlags(
    Mask<FWL_EVENTFLAG> dwFlag) {
  Mask<XFA_FWL_KeyFlag> dwFWLFlag;

  if (dwFlag & FWL_EVENTFLAG_ControlKey)
    dwFWLFlag |= XFA_FWL_KeyFlag::kCtrl;
  if (dwFlag & FWL_EVENTFLAG_LeftButtonDown)
    dwFWLFlag |= XFA_FWL_KeyFlag::kLButton;
  if (dwFlag & FWL_EVENTFLAG_MiddleButtonDown)
    dwFWLFlag |= XFA_FWL_KeyFlag::kMButton;
  if (dwFlag & FWL_EVENTFLAG_RightButtonDown)
    dwFWLFlag |= XFA_FWL_KeyFlag::kRButton;
  if (dwFlag & FWL_EVENTFLAG_ShiftKey)
    dwFWLFlag |= XFA_FWL_KeyFlag::kShift;
  if (dwFlag & FWL_EVENTFLAG_AltKey)
    dwFWLFlag |= XFA_FWL_KeyFlag::kAlt;

  return dwFWLFlag;
}
