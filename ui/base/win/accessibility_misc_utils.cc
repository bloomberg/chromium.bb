// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/base/win/accessibility_misc_utils.h"

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "ui/base/win/atl_module.h"

namespace base {
namespace win {

void SetInvisibleSystemCaretRect(HWND hwnd, const gfx::Rect& caret_rect) {
  // Create an invisible bitmap.
  base::win::ScopedGDIObject<HBITMAP> caret_bitmap(
      CreateBitmap(1, caret_rect.height(), 1, 1, NULL));

  // This destroys the previous caret (no matter what window it belonged to)
  // and creates a new one owned by this window.
  if (!CreateCaret(hwnd, caret_bitmap, 1, caret_rect.height()))
    return;

  ShowCaret(hwnd);
  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  SetCaretPos(caret_rect.x() - window_rect.left + 2,
              caret_rect.y() - window_rect.top + 2);
}

// UIA TextProvider implementation.
UIATextProvider::UIATextProvider()
    : editable_(false) {}

// static
bool UIATextProvider::CreateTextProvider(bool editable, IUnknown** provider) {
  // Make sure ATL is initialized in this module.
  ui::win::CreateATLModuleIfNeeded();

  CComObject<UIATextProvider>* text_provider = NULL;
  HRESULT hr = CComObject<UIATextProvider>::CreateInstance(&text_provider);
  if (SUCCEEDED(hr)) {
    DCHECK(text_provider);
    text_provider->set_editable(editable);
    text_provider->AddRef();
    *provider = static_cast<ITextProvider*>(text_provider);
    return true;
  }
  return false;
}

STDMETHODIMP UIATextProvider::get_IsReadOnly(BOOL* read_only) {
  *read_only = !editable_;
  return S_OK;
}

}  // namespace win
}  // namespace base
