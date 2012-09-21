// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_TSF_INPUT_SCOPE_H_
#define UI_BASE_WIN_TSF_INPUT_SCOPE_H_

#include <InputScope.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

namespace ui {

// The implementation class of ITfInputScope, which is the Windows-specific
// category representation corresponding to ui::TextInputType that we are using
// to specify the expected text type in the target field.
class TsfInputScope : public ITfInputScope {
 public:
  explicit TsfInputScope(TextInputType text_input_type);
  virtual ~TsfInputScope();

  // ITfInputScope override.
  virtual ULONG STDMETHODCALLTYPE AddRef() OVERRIDE;

  // ITfInputScope override.
  virtual ULONG STDMETHODCALLTYPE Release() OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP QueryInterface(REFIID iid, void** result) OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP GetInputScopes(InputScope** input_scopes,
                                      UINT* count) OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP GetPhrase(BSTR** phrases, UINT* count) OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP GetRegularExpression(BSTR* regexp) OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP GetSRGS(BSTR* srgs) OVERRIDE;

  // ITfInputScope override.
  virtual STDMETHODIMP GetXML(BSTR* xml) OVERRIDE;

 private:
  // The corresponding text input type.
  TextInputType text_input_type_;

  // The refrence count of this instance.
  volatile LONG ref_count_;

  DISALLOW_COPY_AND_ASSIGN(TsfInputScope);
};

}  // namespace ui

#endif  // UI_BASE_WIN_TSF_INPUT_SCOPE_H_
