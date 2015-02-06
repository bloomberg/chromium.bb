// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hud_font.h"

#include "base/lazy_instance.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace ui {

namespace {
base::LazyInstance<skia::RefPtr<SkTypeface>> g_hud_typeface;
}  // namespace

void SetHudTypeface(skia::RefPtr<SkTypeface> typeface) {
  g_hud_typeface.Get() = typeface;
}

skia::RefPtr<SkTypeface> GetHudTypeface() {
  if (!g_hud_typeface.Get()) {
    // This will be set pre-sandbox on Windows. On other platforms, just make
    // something here.
    SetHudTypeface(skia::AdoptRef(
        SkTypeface::CreateFromName("monospace", SkTypeface::kBold)));
  }
  return g_hud_typeface.Get();
}

}  // namespace ui
