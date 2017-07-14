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

class LayoutLocale;
class FontCache;

using LayoutLocaleMap =
    HashMap<AtomicString, RefPtr<LayoutLocale>, CaseFoldingHash>;

typedef HashMap<FontCache::FontFileKey,
                RefPtr<OpenTypeVerticalData>,
                IntHash<FontCache::FontFileKey>,
                WTF::UnsignedWithZeroKeyHashTraits<FontCache::FontFileKey>>
    FontVerticalDataCache;

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

  static inline LayoutLocaleMap& GetLayoutLocaleMap() {
    return Get()->layout_locale_map_;
  }

  static inline const LayoutLocale* GetDefaultLayoutLocale() {
    return Get()->default_locale_;
  }
  static inline void SetDefaultLayoutLocale(const LayoutLocale* locale) {
    Get()->default_locale_ = locale;
  }
  static inline const LayoutLocale* GetSystemLayoutLocale() {
    return Get()->system_locale_;
  }
  static inline void SetSystemLayoutLocale(const LayoutLocale* locale) {
    Get()->system_locale_ = locale;
  }

  static inline const LayoutLocale* GetDefaultLocaleForHan() {
    FontGlobalContext* ctx = Get();
    DCHECK(ctx->has_default_locale_for_han_);
    return ctx->default_locale_for_han_;
  }
  static inline void SetDefaultLocaleForHan(const LayoutLocale* locale) {
    FontGlobalContext* ctx = Get();
    ctx->default_locale_for_han_ = locale;
    ctx->has_default_locale_for_han_ = true;
  }
  static inline void InvalidateLocaleForHan() {
    FontGlobalContext* ctx = Get();
    ctx->default_locale_for_han_ = nullptr;
    ctx->has_default_locale_for_han_ = false;
  }
  static inline bool HasDefaultLocaleForHan() {
    return Get()->has_default_locale_for_han_;
  }

  static inline String& CurrentAcceptLanguages() {
    return Get()->current_accept_languages_;
  }

  static inline FontVerticalDataCache& GetFontVerticalDataCache() {
    return Get()->font_vertical_data_cache_;
  }

  // Called by MemoryCoordinator to clear memory.
  static void ClearMemory();

  static void ClearForTesting();

 private:
  friend class WTF::ThreadSpecific<FontGlobalContext>;

  FontGlobalContext();
  ~FontGlobalContext() {}

  FontCache font_cache_;

  HarfBuzzFontCache harf_buzz_font_cache_;

  hb_font_funcs_t* harfbuzz_font_funcs_;

  LayoutLocaleMap layout_locale_map_;
  const LayoutLocale* default_locale_;
  const LayoutLocale* system_locale_;
  const LayoutLocale* default_locale_for_han_;
  bool has_default_locale_for_han_;

  String current_accept_languages_;

  FontVerticalDataCache font_vertical_data_cache_;
};

}  // namespace blink

#endif
