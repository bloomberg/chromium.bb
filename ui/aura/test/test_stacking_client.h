// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_STACKING_CLIENT_H_
#define UI_AURA_TEST_TEST_STACKING_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/toplevel_window_container.h"

namespace aura {
class ToplevelWindowContainer;

namespace test {

class TestStackingClient : public StackingClient {
 public:
  // Callers should allocate a TestStackingClient on the heap and then forget
  // about it -- the c'tor passes ownership of the TestStackingClient to the
  // static Desktop object.
  TestStackingClient();
  virtual ~TestStackingClient();

  Window* default_container() { return default_container_.get(); }

 private:
  // Overridden from StackingClient:
  virtual void AddChildToDefaultParent(Window* window) OVERRIDE;
  virtual Window* GetTopmostWindowToActivate(Window* ignore) const OVERRIDE;

  scoped_ptr<ToplevelWindowContainer> default_container_;

  DISALLOW_COPY_AND_ASSIGN(TestStackingClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_STACKING_CLIENT_H_
