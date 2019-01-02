// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_ui_constants.h"

#include "base/no_destructor.h"
#include "ui/gfx/font_list.h"

namespace ash {
namespace assistant {
namespace ui {

const gfx::FontList& GetDefaultFontList() {
  static const base::NoDestructor<gfx::FontList> font_list("Google Sans, 12px");
  return *font_list;
}

}  // namespace ui
}  // namespace assistant
}  // namespace ash