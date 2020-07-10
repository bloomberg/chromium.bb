// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_FORMCONTROL_H_
#define CORE_FPDFDOC_CPDF_FORMCONTROL_H_

#include "core/fpdfdoc/cpdf_aaction.h"
#include "core/fpdfdoc/cpdf_action.h"
#include "core/fpdfdoc/cpdf_annot.h"
#include "core/fpdfdoc/cpdf_annotlist.h"
#include "core/fpdfdoc/cpdf_apsettings.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "core/fpdfdoc/cpdf_iconfit.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/retain_ptr.h"
#include "core/fxge/fx_dib.h"
#include "third_party/base/optional.h"

class CFX_RenderDevice;
class CPDF_Dictionary;
class CPDF_Font;
class CPDF_FormField;
class CPDF_InteractiveForm;
class CPDF_OCContext;
class CPDF_RenderOptions;
class CPDF_Stream;

class CPDF_FormControl {
 public:
  enum HighlightingMode { None = 0, Invert, Outline, Push, Toggle };

  CPDF_FormControl(CPDF_FormField* pField, CPDF_Dictionary* pWidgetDict);
  ~CPDF_FormControl();

  CPDF_FormField::Type GetType() const { return m_pField->GetType(); }
  const CPDF_InteractiveForm* GetInteractiveForm() const {
    return m_pForm.Get();
  }
  CPDF_FormField* GetField() const { return m_pField.Get(); }
  CPDF_Dictionary* GetWidget() const { return m_pWidgetDict.Get(); }
  CFX_FloatRect GetRect() const;

  ByteString GetCheckedAPState() const;
  WideString GetExportValue() const;

  bool IsChecked() const;
  bool IsDefaultChecked() const;

  HighlightingMode GetHighlightingMode() const;
  bool HasMKEntry(const ByteString& csEntry) const;
  int GetRotation() const;

  FX_ARGB GetBorderColor(int& iColorType) { return GetColor(iColorType, "BC"); }

  float GetOriginalBorderColor(int index) {
    return GetOriginalColor(index, "BC");
  }

  void GetOriginalBorderColor(int& iColorType, float fc[4]) {
    GetOriginalColor(iColorType, fc, "BC");
  }

  FX_ARGB GetBackgroundColor(int& iColorType) {
    return GetColor(iColorType, "BG");
  }

  float GetOriginalBackgroundColor(int index) {
    return GetOriginalColor(index, "BG");
  }

  void GetOriginalBackgroundColor(int& iColorType, float fc[4]) {
    GetOriginalColor(iColorType, fc, "BG");
  }

  WideString GetNormalCaption() const { return GetCaption("CA"); }
  WideString GetRolloverCaption() const { return GetCaption("RC"); }
  WideString GetDownCaption() const { return GetCaption("AC"); }

  CPDF_Stream* GetNormalIcon() { return GetIcon("I"); }
  CPDF_Stream* GetRolloverIcon() { return GetIcon("RI"); }
  CPDF_Stream* GetDownIcon() { return GetIcon("IX"); }
  CPDF_IconFit GetIconFit() const;

  int GetTextPosition() const;
  CPDF_Action GetAction() const;
  CPDF_AAction GetAdditionalAction() const;
  CPDF_DefaultAppearance GetDefaultAppearance() const;

  Optional<WideString> GetDefaultControlFontName() const;
  int GetControlAlignment() const;

  ByteString GetOnStateName() const;
  void CheckControl(bool bChecked);

 private:
  RetainPtr<CPDF_Font> GetDefaultControlFont() const;
  FX_ARGB GetColor(int& iColorType, const ByteString& csEntry);
  float GetOriginalColor(int index, const ByteString& csEntry);
  void GetOriginalColor(int& iColorType,
                        float fc[4],
                        const ByteString& csEntry);

  WideString GetCaption(const ByteString& csEntry) const;
  CPDF_Stream* GetIcon(const ByteString& csEntry);
  CPDF_ApSettings GetMK() const;

  UnownedPtr<CPDF_FormField> const m_pField;
  RetainPtr<CPDF_Dictionary> const m_pWidgetDict;
  UnownedPtr<const CPDF_InteractiveForm> const m_pForm;
};

#endif  // CORE_FPDFDOC_CPDF_FORMCONTROL_H_
