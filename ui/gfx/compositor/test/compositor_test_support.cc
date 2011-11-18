// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/test/compositor_test_support.h"

#if defined(USE_WEBKIT_COMPOSITOR)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
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

}  // namespace ui
