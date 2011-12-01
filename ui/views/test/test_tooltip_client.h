// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_TOOLTIP_CLIENT_H_
#define UI_AURA_TEST_TEST_TOOLTIP_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/client/tooltip_client.h"

namespace aura {
class Window;

namespace test {

class TestTooltipClient : public TooltipClient {
 public:
  TestTooltipClient();
  virtual ~TestTooltipClient();

 private:
  // Overridden from TooltipClient:
  virtual void UpdateTooltip(Window* target) OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(TestTooltipClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_TOOLTIP_CLIENT_H_
