// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/directwrite_keepalive_win.h"

#include "base/logging.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/src/ports/SkTypeface_win_dw.h"

void SkiaDirectWriteKeepalive(SkFontMgr* fontmgr) {
  // We can't use a simple SkPaint similar to sandbox warmup here because we
  // need to avoid hitting any of Skia's caches to make sure we actually keep
  // the connection alive. See crbug.com/377932 for more details.
  skia::RefPtr<SkTypeface> typeface =
      skia::AdoptRef(fontmgr->legacyCreateTypeface("Arial", 0));
  DWriteFontTypeface* dwrite_typeface =
      reinterpret_cast<DWriteFontTypeface*>(typeface.get());
  // Cycle through all 65536 to avoid hitting the in-process dwrite.dll cache.
  static uint16_t glyph_id = 0;
  glyph_id += 1;
  DWRITE_GLYPH_METRICS gm;
  HRESULT hr = dwrite_typeface->fDWriteFontFace->GetDesignGlyphMetrics(
      &glyph_id, 1, &gm);
  // If this check fails, we were unable to retrieve metrics from the
  // DirectWrite service, because the ALPC port has been dropped. In this
  // case, we will be unable to retrieve metrics or glyphs, so we might as
  // well crash.
  CHECK(SUCCEEDED(hr));
}
