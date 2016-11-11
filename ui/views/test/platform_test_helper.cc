// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {
namespace {

PlatformTestHelper::Factory test_helper_factory;
bool is_mus = false;
bool is_aura_mus_client = false;

}  // namespace

void PlatformTestHelper::set_factory(const Factory& factory) {
  DCHECK(test_helper_factory.is_null());
  test_helper_factory = factory;
}

// static
std::unique_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return !test_helper_factory.is_null()
             ? test_helper_factory.Run()
             : base::WrapUnique(new PlatformTestHelper);
}

// static
void PlatformTestHelper::SetIsMus() {
  is_mus = true;
}

// static
bool PlatformTestHelper::IsMus() {
  return is_mus;
}

// static
void PlatformTestHelper::SetIsAuraMusClient() {
  is_aura_mus_client = true;
}

// static
bool PlatformTestHelper::IsAuraMusClient() {
  return is_aura_mus_client;
}

#if defined(USE_AURA)
void PlatformTestHelper::SimulateNativeDestroy(Widget* widget) {
  delete widget->GetNativeView();
}
#endif

}  // namespace views
