// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_properties.h"

#include "ui/gfx/geometry/insets.h"

#if !defined(USE_AURA)
// aura_constants.cc also declared the bool ClassProperty type.
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, bool);
#endif

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(VIEWS_EXPORT, gfx::Insets*);

namespace views {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Insets, kMarginsKey, nullptr);

}  // namespace views
