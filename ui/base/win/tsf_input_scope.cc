// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/tsf_input_scope.h"

#include "base/logging.h"

namespace ui {

TsfInputScope::TsfInputScope(TextInputType text_input_type)
  : text_input_type_(text_input_type),
    ref_count_(0) {
}

TsfInputScope::~TsfInputScope() {
}

ULONG STDMETHODCALLTYPE TsfInputScope::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG STDMETHODCALLTYPE TsfInputScope::Release() {
  const LONG count = InterlockedDecrement(&ref_count_);
  if (!count) {
    delete this;
    return 0;
  }
  return static_cast<ULONG>(count);
}

STDMETHODIMP TsfInputScope::QueryInterface(REFIID iid, void** result) {
  if (!result)
    return E_INVALIDARG;
  if (iid == IID_IUnknown || iid == IID_ITfInputScope) {
    *result = static_cast<ITfInputScope*>(this);
  } else {
    *result = NULL;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP TsfInputScope::GetInputScopes(InputScope** input_scopes,
                                           UINT* count) {
  DCHECK_NE(text_input_type_, TEXT_INPUT_TYPE_NONE);
  if (!count || !input_scopes)
    return E_INVALIDARG;
  *input_scopes = static_cast<InputScope*>(CoTaskMemAlloc(sizeof(InputScope)));
  if (!input_scopes) {
    *count = 0;
    return E_OUTOFMEMORY;
  }
  (*input_scopes)[0] =
      (text_input_type_ == TEXT_INPUT_TYPE_PASSWORD) ? IS_PASSWORD : IS_DEFAULT;
  *count = 1;
  return S_OK;
}

STDMETHODIMP TsfInputScope::GetPhrase(BSTR** phrases, UINT* count) {
  return E_NOTIMPL;
}

STDMETHODIMP TsfInputScope::GetRegularExpression(BSTR* regexp) {
  return E_NOTIMPL;
}

STDMETHODIMP TsfInputScope::GetSRGS(BSTR* srgs) {
  return E_NOTIMPL;
}

STDMETHODIMP TsfInputScope::GetXML(BSTR* xml) {
  return E_NOTIMPL;
}

}  // namespace ui
