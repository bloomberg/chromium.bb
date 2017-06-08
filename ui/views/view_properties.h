// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_PROPERTIES_H_
#define UI_VIEWS_VIEW_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/views/views_export.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

// A property to store margins around the outer perimeter of the view. Margins
// are outside the bounds of the view. This is used by various layout managers
// to position views with the proper spacing between them.
VIEWS_EXPORT extern const ui::ClassProperty<gfx::Insets*>* const kMarginsKey;

}  // namespace views

#endif  // UI_VIEWS_VIEW_PROPERTIES_H_
