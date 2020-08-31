// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_BASE_WIN_ACCESSIBILITY_MISC_UTILS_H_
#define UI_BASE_WIN_ACCESSIBILITY_MISC_UTILS_H_

#include "base/win/atl.h"  // Must be before UIAutomationCore.h

#include <UIAutomationCore.h>

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"

namespace base {
namespace win {

  // UIA Text provider implementation for edit controls.
class UI_BASE_EXPORT UIATextProvider
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITextProvider {
 public:
  BEGIN_COM_MAP(UIATextProvider)
    COM_INTERFACE_ENTRY2(IUnknown, ITextProvider)
    COM_INTERFACE_ENTRY(ITextProvider)
  END_COM_MAP()

  UIATextProvider();
  ~UIATextProvider();

  // Creates an instance of the UIATextProvider class.
  // Returns true on success
  static bool CreateTextProvider(const string16& value,
                                 bool editable,
                                 IUnknown** provider);

  void set_editable(bool editable) {
    editable_ = editable;
  }

  void set_value(const string16& value) { value_ = value; }

  //
  // ITextProvider methods.
  //
  IFACEMETHODIMP GetSelection(SAFEARRAY** ret) override;

  IFACEMETHODIMP GetVisibleRanges(SAFEARRAY** ret) override;

  IFACEMETHODIMP RangeFromChild(IRawElementProviderSimple* child,
                                ITextRangeProvider** ret) override;

  IFACEMETHODIMP RangeFromPoint(struct UiaPoint point,
                                ITextRangeProvider** ret) override;

  IFACEMETHODIMP get_DocumentRange(ITextRangeProvider** ret) override;

  IFACEMETHODIMP get_SupportedTextSelection(
      enum SupportedTextSelection* ret) override;

 private:
  bool editable_;
  string16 value_;
};

}  // win
}  // base

#endif  // UI_BASE_WIN_ACCESSIBILITY_MISC_UTILS_H_
