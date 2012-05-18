// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
#define PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_

#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "webkit/glue/webpreferences.h"

namespace webkit_glue {
struct WebPreferences;
}

namespace ppapi {

struct PPAPI_SHARED_EXPORT Preferences {
 public:
  Preferences();
  explicit Preferences(const webkit_glue::WebPreferences& prefs);
  ~Preferences();

  webkit_glue::WebPreferences::ScriptFontFamilyMap standard_font_family_map;
  webkit_glue::WebPreferences::ScriptFontFamilyMap fixed_font_family_map;
  webkit_glue::WebPreferences::ScriptFontFamilyMap serif_font_family_map;
  webkit_glue::WebPreferences::ScriptFontFamilyMap sans_serif_font_family_map;
  int default_font_size;
  int default_fixed_font_size;

  bool is_3d_supported;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
