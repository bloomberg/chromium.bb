// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_formfield.h"

#include <memory>
#include <set>
#include <utility>

#include "constants/form_fields.h"
#include "constants/form_flags.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/parser/cfdf_document.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/parser/fpdf_parser_utility.h"
#include "core/fpdfdoc/cpdf_defaultappearance.h"
#include "core/fpdfdoc/cpdf_formcontrol.h"
#include "core/fpdfdoc/cpdf_interactiveform.h"
#include "core/fpdfdoc/cpvt_generateap.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

const CPDF_Object* FPDF_GetFieldAttrRecursive(const CPDF_Dictionary* pFieldDict,
                                              const char* name,
                                              int nLevel) {
  static constexpr int kGetFieldMaxRecursion = 32;
  if (!pFieldDict || nLevel > kGetFieldMaxRecursion)
    return nullptr;

  const CPDF_Object* pAttr = pFieldDict->GetDirectObjectFor(name);
  if (pAttr)
    return pAttr;

  return FPDF_GetFieldAttrRecursive(
      pFieldDict->GetDictFor(pdfium::form_fields::kParent), name, nLevel + 1);
}

}  // namespace

Optional<FormFieldType> IntToFormFieldType(int value) {
  if (value >= static_cast<int>(FormFieldType::kUnknown) &&
      value < static_cast<int>(kFormFieldTypeCount)) {
    return {static_cast<FormFieldType>(value)};
  }
  return {};
}

const CPDF_Object* FPDF_GetFieldAttr(const CPDF_Dictionary* pFieldDict,
                                     const char* name) {
  return FPDF_GetFieldAttrRecursive(pFieldDict, name, 0);
}

CPDF_Object* FPDF_GetFieldAttr(CPDF_Dictionary* pFieldDict, const char* name) {
  return const_cast<CPDF_Object*>(FPDF_GetFieldAttrRecursive(
      static_cast<const CPDF_Dictionary*>(pFieldDict), name, 0));
}

WideString FPDF_GetFullName(CPDF_Dictionary* pFieldDict) {
  WideString full_name;
  std::set<CPDF_Dictionary*> visited;
  CPDF_Dictionary* pLevel = pFieldDict;
  while (pLevel) {
    visited.insert(pLevel);
    WideString short_name = pLevel->GetUnicodeTextFor(pdfium::form_fields::kT);
    if (!short_name.IsEmpty()) {
      if (full_name.IsEmpty())
        full_name = std::move(short_name);
      else
        full_name = short_name + L'.' + full_name;
    }
    pLevel = pLevel->GetDictFor(pdfium::form_fields::kParent);
    if (pdfium::ContainsKey(visited, pLevel))
      break;
  }
  return full_name;
}

CPDF_FormField::CPDF_FormField(CPDF_InteractiveForm* pForm,
                               CPDF_Dictionary* pDict)
    : m_pForm(pForm), m_pDict(pDict) {
  InitFieldFlags();
}

CPDF_FormField::~CPDF_FormField() = default;

void CPDF_FormField::InitFieldFlags() {
  const CPDF_Object* ft_attr =
      FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kFT);
  ByteString type_name = ft_attr ? ft_attr->GetString() : ByteString();
  uint32_t flags = GetFieldFlags();
  m_bRequired = flags & pdfium::form_flags::kRequired;
  m_bNoExport = flags & pdfium::form_flags::kNoExport;

  if (type_name == pdfium::form_fields::kBtn) {
    if (flags & pdfium::form_flags::kButtonRadio) {
      m_Type = kRadioButton;
      m_bIsUnison = flags & pdfium::form_flags::kButtonRadiosInUnison;
    } else if (flags & pdfium::form_flags::kButtonPushbutton) {
      m_Type = kPushButton;
    } else {
      m_Type = kCheckBox;
      m_bIsUnison = true;
    }
  } else if (type_name == pdfium::form_fields::kTx) {
    if (flags & pdfium::form_flags::kTextFileSelect)
      m_Type = kFile;
    else if (flags & pdfium::form_flags::kTextRichText)
      m_Type = kRichText;
    else
      m_Type = kText;
    LoadDA();
  } else if (type_name == pdfium::form_fields::kCh) {
    if (flags & pdfium::form_flags::kChoiceCombo) {
      m_Type = kComboBox;
    } else {
      m_Type = kListBox;
      m_bIsMultiSelectListBox = flags & pdfium::form_flags::kChoiceMultiSelect;
    }
    LoadDA();
  } else if (type_name == pdfium::form_fields::kSig) {
    m_Type = kSign;
  }
}

WideString CPDF_FormField::GetFullName() const {
  return FPDF_GetFullName(m_pDict.Get());
}

bool CPDF_FormField::ResetField(NotificationOption notify) {
  switch (m_Type) {
    case kCheckBox:
    case kRadioButton: {
      int iCount = CountControls();
      // TODO(weili): Check whether anything special needs to be done for
      // |m_bIsUnison|.
      for (int i = 0; i < iCount; i++) {
        CheckControl(i, GetControl(i)->IsDefaultChecked(),
                     NotificationOption::kDoNotNotify);
      }
      if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify())
        m_pForm->GetFormNotify()->AfterCheckedStatusChange(this);
      break;
    }
    case kComboBox:
    case kListBox: {
      ClearSelection(NotificationOption::kDoNotNotify);
      WideString csValue;
      int iIndex = GetDefaultSelectedItem();
      if (iIndex >= 0)
        csValue = GetOptionLabel(iIndex);
      if (notify == NotificationOption::kNotify &&
          !NotifyListOrComboBoxBeforeChange(csValue)) {
        return false;
      }
      SetItemSelection(iIndex, true, NotificationOption::kDoNotNotify);
      if (notify == NotificationOption::kNotify)
        NotifyListOrComboBoxAfterChange();
      break;
    }
    case kText:
    case kRichText:
    case kFile:
    default: {
      const CPDF_Object* pDV = GetDefaultValueObject();
      WideString csDValue;
      if (pDV)
        csDValue = pDV->GetUnicodeText();

      WideString csValue;
      {
        // Limit the scope of |pV| because it may get invalidated below.
        const CPDF_Object* pV = GetValueObject();
        if (pV)
          csValue = pV->GetUnicodeText();
      }

      bool bHasRV = !!FPDF_GetFieldAttr(m_pDict.Get(), "RV");
      if (!bHasRV && (csDValue == csValue))
        return false;

      if (notify == NotificationOption::kNotify &&
          !NotifyBeforeValueChange(csDValue)) {
        return false;
      }
      if (pDV) {
        RetainPtr<CPDF_Object> pClone = pDV->Clone();
        if (!pClone)
          return false;

        m_pDict->SetFor(pdfium::form_fields::kV, std::move(pClone));
        if (bHasRV) {
          m_pDict->SetFor("RV", pDV->Clone());
        }
      } else {
        m_pDict->RemoveFor(pdfium::form_fields::kV);
        m_pDict->RemoveFor("RV");
      }
      if (notify == NotificationOption::kNotify)
        NotifyAfterValueChange();
      break;
    }
  }
  return true;
}

int CPDF_FormField::CountControls() const {
  return pdfium::CollectionSize<int>(GetControls());
}

CPDF_FormControl* CPDF_FormField::GetControl(int index) const {
  return GetControls()[index].Get();
}

int CPDF_FormField::GetControlIndex(const CPDF_FormControl* pControl) const {
  if (!pControl)
    return -1;

  const auto& controls = GetControls();
  auto it = std::find(controls.begin(), controls.end(), pControl);
  return it != controls.end() ? it - controls.begin() : -1;
}

FormFieldType CPDF_FormField::GetFieldType() const {
  switch (m_Type) {
    case kPushButton:
      return FormFieldType::kPushButton;
    case kCheckBox:
      return FormFieldType::kCheckBox;
    case kRadioButton:
      return FormFieldType::kRadioButton;
    case kComboBox:
      return FormFieldType::kComboBox;
    case kListBox:
      return FormFieldType::kListBox;
    case kText:
    case kRichText:
    case kFile:
      return FormFieldType::kTextField;
    case kSign:
      return FormFieldType::kSignature;
    default:
      return FormFieldType::kUnknown;
  }
}

CPDF_AAction CPDF_FormField::GetAdditionalAction() const {
  CPDF_Object* pObj =
      FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kAA);
  return CPDF_AAction(pObj ? pObj->GetDict() : nullptr);
}

WideString CPDF_FormField::GetAlternateName() const {
  const CPDF_Object* pObj =
      FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kTU);
  return pObj ? pObj->GetUnicodeText() : WideString();
}

WideString CPDF_FormField::GetMappingName() const {
  const CPDF_Object* pObj =
      FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kTM);
  return pObj ? pObj->GetUnicodeText() : WideString();
}

uint32_t CPDF_FormField::GetFieldFlags() const {
  const CPDF_Object* pObj =
      FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kFf);
  return pObj ? pObj->GetInteger() : 0;
}

ByteString CPDF_FormField::GetDefaultStyle() const {
  const CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict.Get(), "DS");
  return pObj ? pObj->GetString() : ByteString();
}

void CPDF_FormField::SetOpt(RetainPtr<CPDF_Object> pOpt) {
  m_pDict->SetFor("Opt", std::move(pOpt));
}

WideString CPDF_FormField::GetValue(bool bDefault) const {
  if (GetType() == kCheckBox || GetType() == kRadioButton)
    return GetCheckValue(bDefault);

  const CPDF_Object* pValue =
      bDefault ? GetDefaultValueObject() : GetValueObject();
  if (!pValue) {
    if (!bDefault && m_Type != kText)
      pValue = GetDefaultValueObject();
    if (!pValue)
      return WideString();
  }

  switch (pValue->GetType()) {
    case CPDF_Object::kString:
    case CPDF_Object::kStream:
      return pValue->GetUnicodeText();
    case CPDF_Object::kArray:
      pValue = pValue->AsArray()->GetDirectObjectAt(0);
      if (pValue)
        return pValue->GetUnicodeText();
      break;
    default:
      break;
  }
  return WideString();
}

WideString CPDF_FormField::GetValue() const {
  return GetValue(false);
}

WideString CPDF_FormField::GetDefaultValue() const {
  return GetValue(true);
}

bool CPDF_FormField::SetValue(const WideString& value,
                              bool bDefault,
                              NotificationOption notify) {
  switch (m_Type) {
    case kCheckBox:
    case kRadioButton: {
      SetCheckValue(value, bDefault, notify);
      return true;
    }
    case kFile:
    case kRichText:
    case kText:
    case kComboBox: {
      WideString csValue = value;
      if (notify == NotificationOption::kNotify &&
          !NotifyBeforeValueChange(csValue)) {
        return false;
      }
      ByteString key(bDefault ? pdfium::form_fields::kDV
                              : pdfium::form_fields::kV);
      m_pDict->SetNewFor<CPDF_String>(key, csValue);
      int iIndex = FindOption(csValue);
      if (iIndex < 0) {
        if (m_Type == kRichText && !bDefault) {
          m_pDict->SetFor("RV", m_pDict->GetObjectFor(key)->Clone());
        }
        m_pDict->RemoveFor("I");
      } else {
        if (!bDefault) {
          ClearSelection(NotificationOption::kDoNotNotify);
          SetItemSelection(iIndex, true, NotificationOption::kDoNotNotify);
        }
      }
      if (notify == NotificationOption::kNotify)
        NotifyAfterValueChange();
      break;
    }
    case kListBox: {
      int iIndex = FindOption(value);
      if (iIndex < 0)
        return false;

      if (bDefault && iIndex == GetDefaultSelectedItem())
        return false;

      if (notify == NotificationOption::kNotify &&
          !NotifyBeforeSelectionChange(value)) {
        return false;
      }
      if (!bDefault) {
        ClearSelection(NotificationOption::kDoNotNotify);
        SetItemSelection(iIndex, true, NotificationOption::kDoNotNotify);
      }
      if (notify == NotificationOption::kNotify)
        NotifyAfterSelectionChange();
      break;
    }
    default:
      break;
  }
  return true;
}

bool CPDF_FormField::SetValue(const WideString& value,
                              NotificationOption notify) {
  return SetValue(value, false, notify);
}

int CPDF_FormField::GetMaxLen() const {
  if (const CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict.Get(), "MaxLen"))
    return pObj->GetInteger();

  for (auto& pControl : GetControls()) {
    if (!pControl)
      continue;

    const CPDF_Dictionary* pWidgetDict = pControl->GetWidget();
    if (pWidgetDict->KeyExist("MaxLen"))
      return pWidgetDict->GetIntegerFor("MaxLen");
  }
  return 0;
}

int CPDF_FormField::CountSelectedItems() const {
  const CPDF_Object* pValue = GetValueOrSelectedIndicesObject();
  if (!pValue)
    return 0;

  if (pValue->IsString() || pValue->IsNumber())
    return pValue->GetString().IsEmpty() ? 0 : 1;
  const CPDF_Array* pArray = pValue->AsArray();
  return pArray ? pArray->size() : 0;
}

int CPDF_FormField::GetSelectedIndex(int index) const {
  const CPDF_Object* pValue = GetValueOrSelectedIndicesObject();
  if (!pValue)
    return -1;

  if (pValue->IsNumber())
    return pValue->GetInteger();

  WideString sel_value;
  if (pValue->IsString()) {
    if (index != 0)
      return -1;
    sel_value = pValue->GetUnicodeText();
  } else {
    const CPDF_Array* pArray = pValue->AsArray();
    if (!pArray || index < 0)
      return -1;

    const CPDF_Object* elementValue = pArray->GetDirectObjectAt(index);
    sel_value = elementValue ? elementValue->GetUnicodeText() : WideString();
  }
  if (index < CountSelectedOptions()) {
    int iOptIndex = GetSelectedOptionIndex(index);
    WideString csOpt = GetOptionValue(iOptIndex);
    if (csOpt == sel_value)
      return iOptIndex;
  }
  for (int i = 0; i < CountOptions(); i++) {
    if (sel_value == GetOptionValue(i))
      return i;
  }
  return -1;
}

bool CPDF_FormField::ClearSelection(NotificationOption notify) {
  if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify()) {
    WideString csValue;
    int iIndex = GetSelectedIndex(0);
    if (iIndex >= 0)
      csValue = GetOptionLabel(iIndex);
    if (!NotifyListOrComboBoxBeforeChange(csValue))
      return false;
  }
  m_pDict->RemoveFor(pdfium::form_fields::kV);
  m_pDict->RemoveFor("I");
  if (notify == NotificationOption::kNotify)
    NotifyListOrComboBoxAfterChange();
  return true;
}

bool CPDF_FormField::IsItemSelected(int index) const {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  if (index < 0 || index >= CountOptions())
    return false;
  if (IsOptionSelected(index))
    return true;

  WideString opt_value = GetOptionValue(index);
  const CPDF_Object* pValue = GetValueOrSelectedIndicesObject();
  if (!pValue)
    return false;

  if (pValue->IsString())
    return pValue->GetUnicodeText() == opt_value;

  if (pValue->IsNumber()) {
    if (pValue->GetString().IsEmpty())
      return false;
    return (pValue->GetInteger() == index);
  }

  const CPDF_Array* pArray = pValue->AsArray();
  if (!pArray)
    return false;

  for (int i = 0; i < CountSelectedOptions(); ++i) {
    if (GetSelectedOptionIndex(i) == index) {
      const CPDF_Object* pDirectObj = pArray->GetDirectObjectAt(i);
      return pDirectObj && pDirectObj->GetUnicodeText() == opt_value;
    }
  }
  return false;
}

bool CPDF_FormField::SetItemSelection(int index,
                                      bool bSelected,
                                      NotificationOption notify) {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  if (index < 0 || index >= CountOptions())
    return false;

  WideString opt_value = GetOptionValue(index);
  if (notify == NotificationOption::kNotify &&
      !NotifyListOrComboBoxBeforeChange(opt_value)) {
    return false;
  }

  if (bSelected)
    SetItemSelectionSelected(index, opt_value);
  else
    SetItemSelectionUnselected(index, opt_value);

  if (notify == NotificationOption::kNotify)
    NotifyListOrComboBoxAfterChange();
  return true;
}

void CPDF_FormField::SetItemSelectionSelected(int index,
                                              const WideString& opt_value) {
  if (GetType() != kListBox) {
    m_pDict->SetNewFor<CPDF_String>(pdfium::form_fields::kV, opt_value);
    CPDF_Array* pI = m_pDict->SetNewFor<CPDF_Array>("I");
    pI->AddNew<CPDF_Number>(index);
    return;
  }

  SelectOption(index, true, NotificationOption::kDoNotNotify);
  if (!m_bIsMultiSelectListBox) {
    m_pDict->SetNewFor<CPDF_String>(pdfium::form_fields::kV, opt_value);
    return;
  }

  CPDF_Array* pArray = m_pDict->SetNewFor<CPDF_Array>(pdfium::form_fields::kV);
  for (int i = 0; i < CountOptions(); i++) {
    if (i == index || IsItemSelected(i))
      pArray->AddNew<CPDF_String>(GetOptionValue(i));
  }
}

void CPDF_FormField::SetItemSelectionUnselected(int index,
                                                const WideString& opt_value) {
  const CPDF_Object* pValue = GetValueObject();
  if (!pValue)
    return;

  if (GetType() != kListBox) {
    m_pDict->RemoveFor(pdfium::form_fields::kV);
    m_pDict->RemoveFor("I");
    return;
  }

  SelectOption(index, false, NotificationOption::kDoNotNotify);
  if (pValue->IsString()) {
    if (pValue->GetUnicodeText() == opt_value) {
      m_pDict->RemoveFor(pdfium::form_fields::kV);
    }
    return;
  }

  if (!pValue->IsArray())
    return;

  auto pArray = pdfium::MakeRetain<CPDF_Array>();
  for (int i = 0; i < CountOptions(); i++) {
    if (i != index && IsItemSelected(i))
      pArray->AddNew<CPDF_String>(GetOptionValue(i));
  }
  if (pArray->size() > 0) {
    m_pDict->SetFor(pdfium::form_fields::kV, pArray);
  }
}

bool CPDF_FormField::IsItemDefaultSelected(int index) const {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  if (index < 0 || index >= CountOptions())
    return false;
  int iDVIndex = GetDefaultSelectedItem();
  return iDVIndex >= 0 && iDVIndex == index;
}

int CPDF_FormField::GetDefaultSelectedItem() const {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  const CPDF_Object* pValue = GetDefaultValueObject();
  if (!pValue)
    return -1;
  WideString csDV = pValue->GetUnicodeText();
  if (csDV.IsEmpty())
    return -1;
  for (int i = 0; i < CountOptions(); i++) {
    if (csDV == GetOptionValue(i))
      return i;
  }
  return -1;
}

int CPDF_FormField::CountOptions() const {
  const CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict.Get(), "Opt"));
  return pArray ? pArray->size() : 0;
}

WideString CPDF_FormField::GetOptionText(int index, int sub_index) const {
  const CPDF_Array* pArray = ToArray(FPDF_GetFieldAttr(m_pDict.Get(), "Opt"));
  if (!pArray)
    return WideString();

  const CPDF_Object* pOption = pArray->GetDirectObjectAt(index);
  if (!pOption)
    return WideString();
  if (const CPDF_Array* pOptionArray = pOption->AsArray())
    pOption = pOptionArray->GetDirectObjectAt(sub_index);

  const CPDF_String* pString = ToString(pOption);
  return pString ? pString->GetUnicodeText() : WideString();
}

WideString CPDF_FormField::GetOptionLabel(int index) const {
  return GetOptionText(index, 1);
}

WideString CPDF_FormField::GetOptionValue(int index) const {
  return GetOptionText(index, 0);
}

int CPDF_FormField::FindOption(const WideString& csOptValue) const {
  for (int i = 0; i < CountOptions(); i++) {
    if (GetOptionValue(i) == csOptValue)
      return i;
  }
  return -1;
}

bool CPDF_FormField::CheckControl(int iControlIndex,
                                  bool bChecked,
                                  NotificationOption notify) {
  ASSERT(GetType() == kCheckBox || GetType() == kRadioButton);
  CPDF_FormControl* pControl = GetControl(iControlIndex);
  if (!pControl)
    return false;
  if (!bChecked && pControl->IsChecked() == bChecked)
    return false;

  const WideString csWExport = pControl->GetExportValue();
  int iCount = CountControls();
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pCtrl = GetControl(i);
    if (m_bIsUnison) {
      WideString csEValue = pCtrl->GetExportValue();
      if (csEValue == csWExport) {
        if (pCtrl->GetOnStateName() == pControl->GetOnStateName())
          pCtrl->CheckControl(bChecked);
        else if (bChecked)
          pCtrl->CheckControl(false);
      } else if (bChecked) {
        pCtrl->CheckControl(false);
      }
    } else {
      if (i == iControlIndex)
        pCtrl->CheckControl(bChecked);
      else if (bChecked)
        pCtrl->CheckControl(false);
    }
  }

  const CPDF_Object* pOpt = FPDF_GetFieldAttr(m_pDict.Get(), "Opt");
  if (!ToArray(pOpt)) {
    ByteString csBExport = PDF_EncodeText(csWExport);
    if (bChecked) {
      m_pDict->SetNewFor<CPDF_Name>(pdfium::form_fields::kV, csBExport);
    } else {
      ByteString csV;
      const CPDF_Object* pV = GetValueObject();
      if (pV)
        csV = pV->GetString();
      if (csV == csBExport)
        m_pDict->SetNewFor<CPDF_Name>(pdfium::form_fields::kV, "Off");
    }
  } else if (bChecked) {
    m_pDict->SetNewFor<CPDF_Name>(pdfium::form_fields::kV,
                                  ByteString::Format("%d", iControlIndex));
  }
  if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify())
    m_pForm->GetFormNotify()->AfterCheckedStatusChange(this);
  return true;
}

WideString CPDF_FormField::GetCheckValue(bool bDefault) const {
  ASSERT(GetType() == kCheckBox || GetType() == kRadioButton);
  WideString csExport = L"Off";
  int iCount = CountControls();
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pControl = GetControl(i);
    bool bChecked =
        bDefault ? pControl->IsDefaultChecked() : pControl->IsChecked();
    if (bChecked) {
      csExport = pControl->GetExportValue();
      break;
    }
  }
  return csExport;
}

bool CPDF_FormField::SetCheckValue(const WideString& value,
                                   bool bDefault,
                                   NotificationOption notify) {
  ASSERT(GetType() == kCheckBox || GetType() == kRadioButton);
  int iCount = CountControls();
  for (int i = 0; i < iCount; i++) {
    CPDF_FormControl* pControl = GetControl(i);
    WideString csExport = pControl->GetExportValue();
    bool val = csExport == value;
    if (!bDefault) {
      CheckControl(GetControlIndex(pControl), val,
                   NotificationOption::kDoNotNotify);
    }
    if (val)
      break;
  }
  if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify())
    m_pForm->GetFormNotify()->AfterCheckedStatusChange(this);
  return true;
}

int CPDF_FormField::GetTopVisibleIndex() const {
  const CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict.Get(), "TI");
  return pObj ? pObj->GetInteger() : 0;
}

int CPDF_FormField::CountSelectedOptions() const {
  const CPDF_Array* pArray = ToArray(GetSelectedIndicesObject());
  return pArray ? pArray->size() : 0;
}

int CPDF_FormField::GetSelectedOptionIndex(int index) const {
  const CPDF_Array* pArray = ToArray(GetSelectedIndicesObject());
  if (!pArray)
    return -1;

  int iCount = pArray->size();
  if (iCount < 0 || index >= iCount)
    return -1;
  return pArray->GetIntegerAt(index);
}

bool CPDF_FormField::IsOptionSelected(int iOptIndex) const {
  const CPDF_Array* pArray = ToArray(GetSelectedIndicesObject());
  if (!pArray)
    return false;

  CPDF_ArrayLocker locker(pArray);
  for (const auto& pObj : locker) {
    if (pObj->GetInteger() == iOptIndex)
      return true;
  }
  return false;
}

bool CPDF_FormField::SelectOption(int iOptIndex,
                                  bool bSelected,
                                  NotificationOption notify) {
  CPDF_Array* pArray = m_pDict->GetArrayFor("I");
  if (!pArray) {
    if (!bSelected)
      return true;

    pArray = m_pDict->SetNewFor<CPDF_Array>("I");
  }

  bool bReturn = false;
  for (size_t i = 0; i < pArray->size(); i++) {
    int iFind = pArray->GetIntegerAt(i);
    if (iFind == iOptIndex) {
      if (bSelected)
        return true;

      if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify()) {
        WideString csValue = GetOptionLabel(iOptIndex);
        if (!NotifyListOrComboBoxBeforeChange(csValue))
          return false;
      }
      pArray->RemoveAt(i);
      bReturn = true;
      break;
    }

    if (iFind > iOptIndex) {
      if (!bSelected)
        continue;

      if (notify == NotificationOption::kNotify && m_pForm->GetFormNotify()) {
        WideString csValue = GetOptionLabel(iOptIndex);
        if (!NotifyListOrComboBoxBeforeChange(csValue))
          return false;
      }
      pArray->InsertNewAt<CPDF_Number>(i, iOptIndex);
      bReturn = true;
      break;
    }
  }
  if (!bReturn) {
    if (bSelected)
      pArray->AddNew<CPDF_Number>(iOptIndex);
    if (pArray->IsEmpty())
      m_pDict->RemoveFor("I");
  }
  if (notify == NotificationOption::kNotify)
    NotifyListOrComboBoxAfterChange();

  return true;
}

void CPDF_FormField::LoadDA() {
  CPDF_Dictionary* pFormDict = m_pForm->GetFormDict();
  if (!pFormDict)
    return;

  ByteString DA;
  if (const CPDF_Object* pObj = FPDF_GetFieldAttr(m_pDict.Get(), "DA"))
    DA = pObj->GetString();

  if (DA.IsEmpty())
    DA = pFormDict->GetStringFor("DA");

  if (DA.IsEmpty())
    return;

  CPDF_Dictionary* pDR = pFormDict->GetDictFor("DR");
  if (!pDR)
    return;

  CPDF_Dictionary* pFont = pDR->GetDictFor("Font");
  if (!ValidateFontResourceDict(pFont))
    return;

  CPDF_DefaultAppearance appearance(DA);
  Optional<ByteString> font_name = appearance.GetFont(&m_FontSize);
  if (!font_name)
    return;

  CPDF_Dictionary* pFontDict = pFont->GetDictFor(*font_name);
  if (!pFontDict)
    return;

  auto* pData = CPDF_DocPageData::FromDocument(m_pForm->GetDocument());
  m_pFont = pData->GetFont(pFontDict);
}

bool CPDF_FormField::NotifyBeforeSelectionChange(const WideString& value) {
  auto* pNotify = m_pForm->GetFormNotify();
  return !pNotify || pNotify->BeforeSelectionChange(this, value);
}

void CPDF_FormField::NotifyAfterSelectionChange() {
  auto* pNotify = m_pForm->GetFormNotify();
  if (pNotify)
    pNotify->AfterSelectionChange(this);
}

bool CPDF_FormField::NotifyBeforeValueChange(const WideString& value) {
  auto* pNotify = m_pForm->GetFormNotify();
  return !pNotify || pNotify->BeforeValueChange(this, value);
}

void CPDF_FormField::NotifyAfterValueChange() {
  auto* pNotify = m_pForm->GetFormNotify();
  if (pNotify)
    pNotify->AfterValueChange(this);
}

bool CPDF_FormField::NotifyListOrComboBoxBeforeChange(const WideString& value) {
  switch (GetType()) {
    case kListBox:
      return NotifyBeforeSelectionChange(value);
    case kComboBox:
      return NotifyBeforeValueChange(value);
    default:
      return true;
  }
}

void CPDF_FormField::NotifyListOrComboBoxAfterChange() {
  switch (GetType()) {
    case kListBox:
      NotifyAfterSelectionChange();
      break;
    case kComboBox:
      NotifyAfterValueChange();
      break;
    default:
      break;
  }
}

const CPDF_Object* CPDF_FormField::GetDefaultValueObject() const {
  return FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kDV);
}

const CPDF_Object* CPDF_FormField::GetValueObject() const {
  return FPDF_GetFieldAttr(m_pDict.Get(), pdfium::form_fields::kV);
}

const CPDF_Object* CPDF_FormField::GetSelectedIndicesObject() const {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  return FPDF_GetFieldAttr(m_pDict.Get(), "I");
}

const CPDF_Object* CPDF_FormField::GetValueOrSelectedIndicesObject() const {
  ASSERT(GetType() == kComboBox || GetType() == kListBox);
  const CPDF_Object* pValue = GetValueObject();
  return pValue ? pValue : GetSelectedIndicesObject();
}

const std::vector<UnownedPtr<CPDF_FormControl>>& CPDF_FormField::GetControls()
    const {
  return m_pForm->GetControlsForField(this);
}
