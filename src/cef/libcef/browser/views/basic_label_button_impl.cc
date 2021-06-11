// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/basic_label_button_impl.h"

#include "libcef/browser/views/basic_label_button_view.h"

// static
CefRefPtr<CefLabelButton> CefLabelButton::CreateLabelButton(
    CefRefPtr<CefButtonDelegate> delegate,
    const CefString& text) {
  return CefBasicLabelButtonImpl::Create(delegate, text);
}

// static
CefRefPtr<CefBasicLabelButtonImpl> CefBasicLabelButtonImpl::Create(
    CefRefPtr<CefButtonDelegate> delegate,
    const CefString& text) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefBasicLabelButtonImpl> label_button =
      new CefBasicLabelButtonImpl(delegate);
  label_button->Initialize();
  if (!text.empty())
    label_button->SetText(text);
  return label_button;
}

CefBasicLabelButtonImpl::CefBasicLabelButtonImpl(
    CefRefPtr<CefButtonDelegate> delegate)
    : ParentClass(delegate) {}

views::LabelButton* CefBasicLabelButtonImpl::CreateRootView() {
  return new CefBasicLabelButtonView(delegate());
}

void CefBasicLabelButtonImpl::InitializeRootView() {
  static_cast<CefBasicLabelButtonView*>(root_view())->Initialize();
}
