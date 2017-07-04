// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontGlobalContext.h"

#include "platform/fonts/FontCache.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/ThreadSpecific.h"

namespace blink {

FontGlobalContext* FontGlobalContext::Get(CreateIfNeeded create_if_needed) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FontGlobalContext*>,
                                  font_persistent, ());
  if (!*font_persistent && create_if_needed == kCreate) {
    *font_persistent = new FontGlobalContext();
  }
  return *font_persistent;
}

FontGlobalContext::FontGlobalContext() : harfbuzz_font_funcs_(nullptr) {}

void FontGlobalContext::ClearMemory() {
  if (!Get(kDoNotCreate))
    return;

  GetFontCache().Invalidate();
}

}  // namespace blink
