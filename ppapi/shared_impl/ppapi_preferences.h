// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
#define PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_

#include "base/string16.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace webkit_glue {
struct WebPreferences;
}

namespace ppapi {

struct PPAPI_SHARED_EXPORT Preferences {
 public:
  Preferences();
  explicit Preferences(const webkit_glue::WebPreferences& prefs);
  ~Preferences();

  string16 standard_font_family;
  string16 fixed_font_family;
  string16 serif_font_family;
  string16 sans_serif_font_family;
  int default_font_size;
  int default_fixed_font_size;

  bool is_3d_supported;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_PREFERENCES_H_
