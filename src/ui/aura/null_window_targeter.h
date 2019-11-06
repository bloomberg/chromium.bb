// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_NULL_WINDOW_TARGETER_H_
#define UI_AURA_NULL_WINDOW_TARGETER_H_

#include "ui/aura/aura_export.h"
#include "ui/aura/window_targeter.h"

namespace aura {

// NullWindowTargeter can be installed on a root window to prevent it from
// dispatching events such as during shutdown.
class AURA_EXPORT NullWindowTargeter : public WindowTargeter {
 public:
  NullWindowTargeter();
  ~NullWindowTargeter() override;

  // EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullWindowTargeter);
};

}  // namespace aura

#endif  // UI_AURA_NULL_WINDOW_TARGETER_H_
