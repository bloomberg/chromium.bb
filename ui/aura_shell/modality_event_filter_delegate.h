// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_MODALITY_EVENT_FILTER_DELEGATE_H_
#define UI_AURA_SHELL_MODALITY_EVENT_FILTER_DELEGATE_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace aura_shell {
namespace internal {

class AURA_SHELL_EXPORT ModalityEventFilterDelegate {
 public:
  // Returns true if |window| can receive the specified event.
  virtual bool CanWindowReceiveEvents(aura::Window* window) = 0;
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_MODALITY_EVENT_FILTER_DELEGATE_H_
