// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FFFIELD_H_
#define XFA_FXFA_CXFA_FFFIELD_H_

#include <memory>

#include "xfa/fwl/cfwl_widget.h"
#include "xfa/fwl/ifwl_widgetdelegate.h"
#include "xfa/fxfa/cxfa_ffpageview.h"
#include "xfa/fxfa/cxfa_ffwidget.h"

class CXFA_FFDropDown;
class CXFA_Node;

#define XFA_MINUI_HEIGHT 4.32f
#define XFA_DEFAULTUI_HEIGHT 2.0f

class CXFA_FFField : public CXFA_FFWidget, public IFWL_WidgetDelegate {
 public:
  enum ShapeOption { kSquareShape = 0, kRoundShape };

  explicit CXFA_FFField(CXFA_Node* pNode);
  ~CXFA_FFField() override;

  virtual CXFA_FFDropDown* AsDropDown();

  // CXFA_FFWidget:
  CXFA_FFField* AsField() override;
  CFX_RectF GetBBox(FocusOption focus) override;
  void RenderWidget(CXFA_Graphics* pGS,
                    const CFX_Matrix& matrix,
                    HighlightOption highlight) override;
  bool IsLoaded() override;
  bool LoadWidget() override;
  bool PerformLayout() override;
  bool AcceptsFocusOnButtonDown(uint32_t dwFlags,
                                const CFX_PointF& point,
                                FWL_MouseCommand command) override;
  bool OnMouseEnter() override;
  bool OnMouseExit() override;
  bool OnLButtonDown(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnLButtonUp(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnLButtonDblClk(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnMouseMove(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnMouseWheel(uint32_t dwFlags,
                    const CFX_PointF& point,
                    const CFX_Vector& delta) override;
  bool OnRButtonDown(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnRButtonUp(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnRButtonDblClk(uint32_t dwFlags, const CFX_PointF& point) override;
  bool OnSetFocus(CXFA_FFWidget* pOldWidget) override WARN_UNUSED_RESULT;
  bool OnKillFocus(CXFA_FFWidget* pNewWidget) override WARN_UNUSED_RESULT;
  bool OnKeyDown(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnKeyUp(uint32_t dwKeyCode, uint32_t dwFlags) override;
  bool OnChar(uint32_t dwChar, uint32_t dwFlags) override;
  FWL_WidgetHit HitTest(const CFX_PointF& point) override;

  // IFWL_WidgetDelegate:
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CXFA_Graphics* pGraphics,
                    const CFX_Matrix& matrix) override;

  void UpdateFWL();
  uint32_t UpdateUIProperty();

 protected:
  bool PtInActiveRect(const CFX_PointF& point) override;

  virtual void SetFWLRect();
  void SetFWLThemeProvider();
  CFWL_Widget* GetNormalWidget();
  const CFWL_Widget* GetNormalWidget() const;
  void SetNormalWidget(std::unique_ptr<CFWL_Widget> widget);
  CFX_PointF FWLToClient(const CFX_PointF& point);
  void LayoutCaption();
  void RenderCaption(CXFA_Graphics* pGS, CFX_Matrix* pMatrix);

  int32_t CalculateOverride();
  int32_t CalculateNode(CXFA_Node* pNode);
  bool ProcessCommittedData();
  virtual bool CommitData();
  virtual bool IsDataChanged();
  void DrawHighlight(CXFA_Graphics* pGS,
                     CFX_Matrix* pMatrix,
                     HighlightOption highlight,
                     ShapeOption shape);
  void DrawFocus(CXFA_Graphics* pGS, CFX_Matrix* pMatrix);
  void SendMessageToFWLWidget(std::unique_ptr<CFWL_Message> pMessage);
  void CapPlacement();
  void CapTopBottomPlacement(const CXFA_Margin* margin,
                             const CFX_RectF& rtWidget,
                             XFA_AttributeValue iCapPlacement);
  void CapLeftRightPlacement(const CXFA_Margin* margin,
                             const CFX_RectF& rtWidget,
                             XFA_AttributeValue iCapPlacement);
  void SetEditScrollOffset();

  CFX_RectF m_UIRect;
  CFX_RectF m_CaptionRect;

 private:
  std::unique_ptr<CFWL_Widget> m_pNormalWidget;
};

inline CXFA_FFDropDown* ToDropDown(CXFA_FFField* field) {
  return field ? field->AsDropDown() : nullptr;
}

#endif  // XFA_FXFA_CXFA_FFFIELD_H_
