// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/command_line.h"
#include "base/containers/mru_cache.h"
#include "base/hash.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/switches.h"

#include <fontconfig/fontconfig.h>

namespace gfx {

namespace {

#if defined(OS_CHROMEOS)
// A device scale factor for an internal display (if any)
// that is used to determine if subpixel positioning should be used.
float device_scale_factor_for_internal_display = 1.0f;
#endif

// Keyed by hashes of FontRenderParamQuery structs from
// HashFontRenderParamsQuery().
typedef base::MRUCache<uint32, FontRenderParams> Cache;

// Number of recent GetFontRenderParams() results to cache.
const size_t kCacheSize = 20;

// A cache and the lock that must be held while accessing it.
// GetFontRenderParams() is called by both the UI thread and the sandbox IPC
// thread.
struct SynchronizedCache {
  SynchronizedCache() : cache(kCacheSize) {}

  base::Lock lock;
  Cache cache;
};

base::LazyInstance<SynchronizedCache>::Leaky g_synchronized_cache =
    LAZY_INSTANCE_INITIALIZER;

bool IsBrowserTextSubpixelPositioningEnabled() {
#if defined(OS_CHROMEOS)
  return device_scale_factor_for_internal_display > 1.0f;
#else
  return false;
#endif
}

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT: return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM: return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:   return FontRenderParams::HINTING_FULL;
    default:             return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:  return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:  return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB: return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR: return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:           return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Queries Fontconfig for rendering settings and updates |params_out| and
// |family_out| (if non-NULL). Returns false on failure.
bool QueryFontconfig(const FontRenderParamsQuery& query,
                     FontRenderParams* params_out,
                     std::string* family_out) {
  FcPattern* pattern = FcPatternCreate();
  CHECK(pattern);

  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

  for (std::vector<std::string>::const_iterator it = query.families.begin();
       it != query.families.end(); ++it) {
    FcPatternAddString(
        pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(it->c_str()));
  }
  if (query.pixel_size > 0)
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, query.pixel_size);
  if (query.point_size > 0)
    FcPatternAddInteger(pattern, FC_SIZE, query.point_size);
  if (query.style >= 0) {
    FcPatternAddInteger(pattern, FC_SLANT,
        (query.style & Font::ITALIC) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddInteger(pattern, FC_WEIGHT,
        (query.style & Font::BOLD) ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL);
  }

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(NULL, pattern, &result);
  FcPatternDestroy(pattern);
  if (!match)
    return false;

  if (family_out) {
    FcChar8* family = NULL;
    FcPatternGetString(match, FC_FAMILY, 0, &family);
    if (family)
      family_out->assign(reinterpret_cast<const char*>(family));
  }

  if (params_out) {
    FcBool fc_antialias = 0;
    if (FcPatternGetBool(match, FC_ANTIALIAS, 0, &fc_antialias) ==
        FcResultMatch) {
      params_out->antialiasing = fc_antialias;
    }

    FcBool fc_autohint = 0;
    if (FcPatternGetBool(match, FC_AUTOHINT, 0, &fc_autohint) ==
        FcResultMatch) {
      params_out->autohinter = fc_autohint;
    }

    FcBool fc_bitmap = 0;
    if (FcPatternGetBool(match, FC_EMBEDDED_BITMAP, 0, &fc_bitmap) ==
        FcResultMatch) {
      params_out->use_bitmaps = fc_bitmap;
    }

    FcBool fc_hinting = 0;
    if (FcPatternGetBool(match, FC_HINTING, 0, &fc_hinting) == FcResultMatch) {
      int fc_hint_style = FC_HINT_NONE;
      if (fc_hinting)
        FcPatternGetInteger(match, FC_HINT_STYLE, 0, &fc_hint_style);
      params_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);
    }

    int fc_rgba = FC_RGBA_NONE;
    if (FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba) == FcResultMatch)
      params_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
  }

  FcPatternDestroy(match);
  return true;
}

// Serialize |query| into a string and hash it to a value suitable for use as a
// cache key.
uint32 HashFontRenderParamsQuery(const FontRenderParamsQuery& query) {
  return base::Hash(base::StringPrintf("%d|%d|%d|%d|%s",
      query.for_web_contents, query.pixel_size, query.point_size, query.style,
      JoinString(query.families, ',').c_str()));
}

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  const uint32 hash = HashFontRenderParamsQuery(query);
  if (!family_out) {
    // The family returned by Fontconfig isn't part of FontRenderParams, so we
    // can only return a value from the cache if it wasn't requested.
    SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();
    base::AutoLock lock(synchronized_cache->lock);
    Cache::const_iterator it = synchronized_cache->cache.Get(hash);
    if (it != synchronized_cache->cache.end()) {
      DVLOG(1) << "Returning cached params for " << hash;
      return it->second;
    }
  } else {
    family_out->clear();
  }
  DVLOG(1) << "Computing params for " << hash
           << (family_out ? " (family requested)" : "");

  // Start with the delegate's settings, but let Fontconfig have the final say.
  FontRenderParams params;
  const LinuxFontDelegate* delegate = LinuxFontDelegate::instance();
  if (delegate)
    params = delegate->GetDefaultFontRenderParams();
  QueryFontconfig(query, &params, family_out);
  if (!params.antialiasing) {
    // Cairo forces full hinting when antialiasing is disabled, since anything
    // less than that looks awful; do the same here. Requesting subpixel
    // rendering or positioning doesn't make sense either.
    params.hinting = FontRenderParams::HINTING_FULL;
    params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;
    params.subpixel_positioning = false;
  } else {
    // Fontconfig doesn't support configuring subpixel positioning; check a
    // flag.
    params.subpixel_positioning =
        query.for_web_contents ?
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableWebkitTextSubpixelPositioning) :
        IsBrowserTextSubpixelPositioningEnabled();

    // To enable subpixel positioning, we need to disable hinting.
    if (params.subpixel_positioning)
      params.hinting = FontRenderParams::HINTING_NONE;
  }

  // Use the first family from the list if Fontconfig didn't suggest a family.
  if (family_out && family_out->empty() && !query.families.empty())
    *family_out = query.families[0];

  // Store the computed struct. It's fine if this overwrites a struct that was
  // cached by a different thread in the meantime; the values should be
  // identical.
  SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();
  base::AutoLock lock(synchronized_cache->lock);
  synchronized_cache->cache.Put(hash, params);

  return params;
}

void ClearFontRenderParamsCacheForTest() {
  SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();
  base::AutoLock lock(synchronized_cache->lock);
  synchronized_cache->cache.Clear();
}

#if defined(OS_CHROMEOS)
void SetFontRenderParamsDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_for_internal_display = device_scale_factor;
}
#endif

}  // namespace gfx
