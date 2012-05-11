// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppapi_preferences.h"

#include "webkit/glue/webpreferences.h"

namespace ppapi {

Preferences::Preferences()
    : default_font_size(0),
      default_fixed_font_size(0),
      is_3d_supported(true) {
}

Preferences::Preferences(const webkit_glue::WebPreferences& prefs)
    : standard_font_family(prefs.standard_font_family),
      fixed_font_family(prefs.fixed_font_family),
      serif_font_family(prefs.serif_font_family),
      sans_serif_font_family(prefs.sans_serif_font_family),
      default_font_size(prefs.default_font_size),
      default_fixed_font_size(prefs.default_fixed_font_size),
      // Pepper 3D support keys off of WebGL which is what the GPU blacklist
      // is applied to.
      is_3d_supported(prefs.experimental_webgl_enabled) {
}

Preferences::~Preferences() {
}

}  // namespace ppapi
