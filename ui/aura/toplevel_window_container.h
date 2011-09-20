// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TOPLEVEL_WINDOW_CONTAINER_H_
#define UI_AURA_TOPLEVEL_WINDOW_CONTAINER_H_
#pragma once

#include "ui/aura/window.h"
#include "ui/aura/aura_export.h"

namespace aura {
namespace internal {

class FocusManager;

// A Window subclass that groups top-level windows.
class AURA_EXPORT ToplevelWindowContainer : public Window {
 public:
  ToplevelWindowContainer();
  virtual ~ToplevelWindowContainer();

  // Overridden from Window:
  virtual bool IsToplevelWindowContainer() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowContainer);
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_TOPLEVEL_WINDOW_CONTAINER_H_
