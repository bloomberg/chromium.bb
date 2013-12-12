// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_WINDOW_STACKING_DELEGATE_H_
#define UI_AURA_CLIENT_WINDOW_STACKING_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace aura {
namespace client {

class AURA_EXPORT WindowStackingClient {
 public:
  virtual ~WindowStackingClient() {}

  // Invoked from the various Window stacking functions. Allows the
  // WindowStackingClient to alter the source, target and/or direction to stack.
  virtual void AdjustStacking(Window** child,
                              Window** target,
                              Window::StackDirection* direction) = 0;
};

// Sets/gets the WindowStackingClient. The setter takes ownership of |client|.
AURA_EXPORT void SetWindowStackingClient(WindowStackingClient* client);
AURA_EXPORT WindowStackingClient* GetWindowStackingClient();

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_WINDOW_STACKING_DELEGATE_H_
