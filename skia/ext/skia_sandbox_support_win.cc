// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia_sandbox_support_win.h"
#include "SkFontHost.h"
#include "SkTypeface_win.h"

static SkiaEnsureTypefaceAccessible g_skia_ensure_typeface_accessible = NULL;

SK_API void SetSkiaEnsureTypefaceAccessible(SkiaEnsureTypefaceAccessible func) {
  // This function is supposed to be called once in process life time.
  SkASSERT(g_skia_ensure_typeface_accessible == NULL);
  g_skia_ensure_typeface_accessible = func;
}

// static
void SkFontHost::EnsureTypefaceAccessible(const SkTypeface& typeface) {
  if (g_skia_ensure_typeface_accessible) {
    LOGFONT lf;
    SkLOGFONTFromTypeface(&typeface, &lf);
    g_skia_ensure_typeface_accessible(lf);
  }
}
