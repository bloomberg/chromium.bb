// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontGlobalContext_h
#define FontGlobalContext_h

#include "platform/PlatformExport.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/HarfBuzzFontCache.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/AtomicStringHash.h"

struct hb_font_funcs_t;

namespace blink {

class FontCache;

enum CreateIfNeeded { kDoNotCreate, kCreate };

// FontGlobalContext contains non-thread-safe, thread-specific data used for
// font formatting.
class PLATFORM_EXPORT FontGlobalContext {
  WTF_MAKE_NONCOPYABLE(FontGlobalContext);

 public:
  static FontGlobalContext* Get(CreateIfNeeded = kCreate);

  static inline FontCache& GetFontCache() { return Get()->font_cache_; }

  static inline HarfBuzzFontCache& GetHarfBuzzFontCache() {
    return Get()->harf_buzz_font_cache_;
  }

  static hb_font_funcs_t* GetHarfBuzzFontFuncs() {
    return Get()->harfbuzz_font_funcs_;
  }

  static void SetHarfBuzzFontFuncs(hb_font_funcs_t* funcs) {
    Get()->harfbuzz_font_funcs_ = funcs;
  }

  // Called by MemoryCoordinator to clear memory.
  static void ClearMemory();

 private:
  friend class WTF::ThreadSpecific<FontGlobalContext>;

  FontGlobalContext();

  FontCache font_cache_;

  HarfBuzzFontCache harf_buzz_font_cache_;

  hb_font_funcs_t* harfbuzz_font_funcs_;
};

}  // namespace blink

#endif
