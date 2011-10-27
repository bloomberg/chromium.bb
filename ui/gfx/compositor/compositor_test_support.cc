// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor_test_support.h"

#if defined(USE_WEBKIT_COMPOSITOR)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#endif

#if defined(VIEWS_COMPOSITOR)
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/test_compositor.h"
#endif

namespace ui {

#if defined(USE_WEBKIT_COMPOSITOR)
static webkit_glue::WebKitPlatformSupportImpl* g_webkit_support;
#endif

void CompositorTestSupport::Initialize() {
#if defined(USE_WEBKIT_COMPOSITOR)
  DCHECK(!g_webkit_support);
  g_webkit_support = new webkit_glue::WebKitPlatformSupportImpl;
  WebKit::initialize(g_webkit_support);
#endif
}

void CompositorTestSupport::Terminate() {
#if defined(USE_WEBKIT_COMPOSITOR)
  DCHECK(g_webkit_support);
  WebKit::shutdown();
  delete g_webkit_support;
  g_webkit_support = NULL;
#endif
}

void CompositorTestSupport::SetupMockCompositor() {
#if defined(USE_WEBKIT_COMPOSITOR)
  // TODO(backer): We've got dependencies in Layer that require
  // WebKit support even though we're mocking the Compositor. We
  // would ideally mock out these unnecessary dependencies as well.
  DCHECK(g_webkit_support);
#endif

#if defined(VIEWS_COMPOSITOR)
  // Use a mock compositor that noops draws.
  ui::Compositor::set_compositor_factory_for_testing(
      ui::TestCompositor::Create);
#endif
}

}  // namespace ui
