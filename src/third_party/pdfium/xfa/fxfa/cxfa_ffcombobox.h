// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_FFCOMBOBOX_H_
#define XFA_FXFA_CXFA_FFCOMBOBOX_H_

#include "v8/include/cppgc/member.h"
#include "xfa/fxfa/cxfa_ffdropdown.h"

class CXFA_EventParam;

class CXFA_FFComboBox final : public CXFA_FFDropDown {
 public:
  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_FFComboBox() override;

  void Trace(cppgc::Visitor* visitor) const override;

  // CXFA_FFDropDown:
  CXFA_FFComboBox* AsComboBox() override;

  // CXFA_FFField
  CFX_RectF GetBBox(FocusOption focus) override;
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;
  bool OnRButtonUp(Mask<XFA_FWL_KeyFlag> dwFlags,
                   const CFX_PointF& point) override;
  [[nodiscard]] bool OnKillFocus(CXFA_FFWidget* pNewWidget) override;
  bool CanUndo() override;
  bool CanRedo() override;
  bool CanCopy() override;
  bool CanCut() override;
  bool CanPaste() override;
  bool CanSelectAll() override;
  bool Undo() override;
  bool Redo() override;
  absl::optional<WideString> Copy() override;
  absl::optional<WideString> Cut() override;
  bool Paste(const WideString& wsPaste) override;
  void SelectAll() override;
  void Delete() override;
  void DeSelect() override;
  WideString GetText() override;
  FormFieldType GetFormFieldType() override;

  // IFWL_WidgetDelegate
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFGAS_GEGraphics* pGraphics,
                    const CFX_Matrix& matrix) override;

  // CXFA_FFDropDown
  void InsertItem(const WideString& wsLabel, int32_t nIndex) override;
  void DeleteItem(int32_t nIndex) override;

  void OpenDropDownList();

  void OnTextChanged(CFWL_Widget* pWidget, const WideString& wsChanged);
  void OnSelectChanged(CFWL_Widget* pWidget, bool bLButtonUp);
  void OnPreOpen(CFWL_Widget* pWidget);
  void OnPostOpen(CFWL_Widget* pWidget);
  void SetItemState(int32_t nIndex, bool bSelected);

 private:
  explicit CXFA_FFComboBox(CXFA_Node* pNode);

  // CXFA_FFField:
  bool PtInActiveRect(const CFX_PointF& point) override;
  bool CommitData() override;
  bool UpdateFWLData() override;
  bool IsDataChanged() override;

  uint32_t GetAlignment();
  void FWLEventSelChange(CXFA_EventParam* pParam);
  WideString GetCurrentText() const;

  WideString m_wsNewValue;
  cppgc::Member<IFWL_WidgetDelegate> m_pOldDelegate;
};

#endif  // XFA_FXFA_CXFA_FFCOMBOBOX_H_
