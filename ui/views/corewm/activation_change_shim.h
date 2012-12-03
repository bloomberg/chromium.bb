// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_ACTIVATION_CHANGE_SHIM_
#define UI_VIEWS_COREWM_ACTIVATION_CHANGE_SHIM_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/views/corewm/focus_change_event.h"
#include "ui/views/views_export.h"

namespace ui {
class EventTarget;
}

namespace views {
namespace corewm {

// A class that converts FocusChangeEvents to calls to an
// ActivationChangeObserver. An interim bandaid that allows us to incrementally
// convert observers to use the new FocusController.
class VIEWS_EXPORT ActivationChangeShim
    : public aura::client::ActivationChangeObserver,
      public ui::EventHandler {
 protected:
  explicit ActivationChangeShim(ui::EventTarget* target);
  virtual ~ActivationChangeShim();

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* active,
                                 aura::Window* old_active) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnEvent(ui::Event* event) OVERRIDE;

 private:
  ui::EventTarget* target_;

  DISALLOW_COPY_AND_ASSIGN(ActivationChangeShim);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_ACTIVATION_CHANGE_SHIM_
