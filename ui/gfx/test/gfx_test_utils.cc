// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/gfx_test_utils.h"

#if defined(VIEWS_COMPOSITOR)
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/test_compositor.h"
#endif

namespace ui {
namespace gfx_test_utils {

void SetupTestCompositor() {
#if defined(VIEWS_COMPOSITOR)
  // Use a mock compositor that noops draws.
  ui::Compositor::set_compositor_factory_for_testing(
      ui::TestCompositor::Create);
#endif
}

}  // namespace gfx_test_utils
}  // namespace ui
