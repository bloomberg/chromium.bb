// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace views {
namespace {

class DefaultPlatformTestHelper : public PlatformTestHelper {
 public:
  DefaultPlatformTestHelper() {}

  ~DefaultPlatformTestHelper() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPlatformTestHelper);
};

PlatformTestHelper::Factory test_helper_factory;
bool is_mus = false;

}  // namespace

void PlatformTestHelper::set_factory(const Factory& factory) {
  DCHECK(test_helper_factory.is_null());
  test_helper_factory = factory;
}

// static
std::unique_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return !test_helper_factory.is_null()
             ? test_helper_factory.Run()
             : base::WrapUnique(new DefaultPlatformTestHelper);
}

// static
void PlatformTestHelper::SetIsMus() {
  is_mus = true;
}

// static
bool PlatformTestHelper::IsMus() {
  return is_mus;
}

}  // namespace views
