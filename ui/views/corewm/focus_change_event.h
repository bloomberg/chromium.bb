// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_FOCUS_CHANGE_EVENT_H_
#define UI_VIEWS_COREWM_FOCUS_CHANGE_EVENT_H_

#include "ui/base/events/event.h"
#include "ui/base/events/event_target.h"
#include "ui/views/views_export.h"

namespace views {
namespace corewm {

// FocusChangeEvent notifies a change in focus or activation state.
// Focus refers to the target that can receive key events.
// Activation is the key "top level window" as defined by the window management
// scheme in use.
// Focus and activation are closely related, and the rules governing their
// change are numerous and complex. The implementation of the dispatch of this
// event is handled by the FocusController. See that class and its unit tests
// for specifics.
class VIEWS_EXPORT FocusChangeEvent : public ui::Event {
 public:
  // An API used by the code in FocusController that dispatches
  // FocusChangeEvents.
  class DispatcherApi : public ui::Event::DispatcherApi {
   public:
    explicit DispatcherApi(FocusChangeEvent* event)
        : ui::Event::DispatcherApi(event), event_(event) {}

    void set_last_focus(ui::EventTarget* last_focus) {
      event_->last_focus_ = last_focus;
    }

   private:
    FocusChangeEvent* event_;

    DISALLOW_COPY_AND_ASSIGN(DispatcherApi);
  };

  // |type| is one of the possible FocusChangeEvent types registered by calling
  // RegisterEventTypes() before instantiating this class. See below.
  explicit FocusChangeEvent(int type);
  virtual ~FocusChangeEvent();

  // Must be called before instantiating this class.
  static void RegisterEventTypes();

  // -ing events are sent when focus or activation is about to be changed. This
  // gives the target and its pre- and post- handlers the ability to abort the
  // or re-target the change before the FocusController makes it.
  static int focus_changing_event_type() { return focus_changing_event_type_; }
  static int activation_changing_event_type() {
    return activation_changing_event_type_;
  }

  // -ed events are sent when focus or activation has been changed in the
  // FocusController. It is possible to stop propagation of this event but that
  // only affects handlers downstream from being notified of the change already
  // made in the FocusController.
  static int focus_changed_event_type() { return focus_changed_event_type_; }
  static int activation_changed_event_type() {
    return activation_changed_event_type_;
  }

  ui::EventTarget* last_focus() { return last_focus_; }

 private:
  FocusChangeEvent();

  // The EventTarget that had focus or activation prior to this event.
  ui::EventTarget* last_focus_;

  static int focus_changing_event_type_;
  static int focus_changed_event_type_;
  static int activation_changing_event_type_;
  static int activation_changed_event_type_;

  DISALLOW_COPY_AND_ASSIGN(FocusChangeEvent);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_FOCUS_CHANGE_EVENT_H_
