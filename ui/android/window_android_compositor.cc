// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/android/window_android_compositor.h"

#include "base/lazy_instance.h"
#include "cc/layers/layer_settings.h"

namespace ui {

namespace {
base::LazyInstance<cc::LayerSettings> g_layer_settings =
    LAZY_INSTANCE_INITIALIZER;
} // anonymous namespace

// static
const cc::LayerSettings& WindowAndroidCompositor::LayerSettings() {
  return g_layer_settings.Get();
}

// static
void WindowAndroidCompositor::SetLayerSettings(
    const cc::LayerSettings& settings) {
  g_layer_settings.Get() = settings;
}

}  // namespace ui
