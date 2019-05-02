// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_write.h"

#include <wrl/client.h>

#include "base/debug/alias.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "skia/ext/fontmgr_default.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/platform_font_win.h"

namespace gfx {
namespace win {

namespace {

// Pointer to the global IDWriteFactory interface.
IDWriteFactory* g_direct_write_factory = nullptr;

void SetDirectWriteFactory(IDWriteFactory* factory) {
  DCHECK(!g_direct_write_factory);
  // We grab a reference on the DirectWrite factory. This reference is
  // leaked, which is ok because skia leaks it as well.
  factory->AddRef();
  g_direct_write_factory = factory;
}

}  // anonymous namespace

void CreateDWriteFactory(IDWriteFactory** factory) {
  Microsoft::WRL::ComPtr<IUnknown> factory_unknown;
  HRESULT hr =
      DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                          factory_unknown.GetAddressOf());
  if (FAILED(hr)) {
    base::debug::Alias(&hr);
    CHECK(false);
    return;
  }
  factory_unknown.CopyTo(factory);
}

void InitializeDirectWrite() {
  static bool tried_dwrite_initialize = false;
  DCHECK(!tried_dwrite_initialize);
  tried_dwrite_initialize = true;

  TRACE_EVENT0("fonts", "gfx::InitializeDirectWrite");

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  CreateDWriteFactory(factory.GetAddressOf());
  CHECK(!!factory);
  SetDirectWriteFactory(factory.Get());

  // The skia call to create a new DirectWrite font manager instance can fail
  // if we are unable to get the system font collection from the DirectWrite
  // factory. The GetSystemFontCollection method in the IDWriteFactory
  // interface fails with E_INVALIDARG on certain Windows 7 gold versions
  // (6.1.7600.*).
  sk_sp<SkFontMgr> direct_write_font_mgr =
      SkFontMgr_New_DirectWrite(factory.Get());
  CHECK(!!direct_write_font_mgr);

  // Override the default skia font manager. This must be called before any
  // use of the skia font manager is done (e.g. before any call to
  // SkFontMgr::RefDefault()).
  skia::OverrideDefaultSkFontMgr(std::move(direct_write_font_mgr));
}

IDWriteFactory* GetDirectWriteFactory() {
  // Some unittests may access this accessor without any previous call to
  // |InitializeDirectWrite|. A call to |InitializeDirectWrite| after this
  // function being called is still invalid.
  if (!g_direct_write_factory)
    InitializeDirectWrite();
  return g_direct_write_factory;
}

}  // namespace win
}  // namespace gfx
