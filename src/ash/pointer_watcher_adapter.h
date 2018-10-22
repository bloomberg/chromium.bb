// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POINTER_WATCHER_ADAPTER_H_
#define ASH_POINTER_WATCHER_ADAPTER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;
class PointerEvent;
}  // namespace ui

namespace views {
class PointerWatcher;
enum class PointerWatcherEventTypes;
}  // namespace views

namespace ash {

// Support for PointerWatchers in ash, implemented with a pre-target
// EventHandler on the Shell.
class ASH_EXPORT PointerWatcherAdapter : public ui::EventHandler {
 public:
  PointerWatcherAdapter();
  ~PointerWatcherAdapter() override;

  // See Shell::AddPointerWatcher() for details.
  void AddPointerWatcher(views::PointerWatcher* watcher,
                         views::PointerWatcherEventTypes events);
  void RemovePointerWatcher(views::PointerWatcher* watcher);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  gfx::Point GetLocationInScreen(const ui::LocatedEvent& event) const;
  gfx::NativeView GetTargetWindow(const ui::LocatedEvent& event) const;

  // Calls OnPointerEventObserved() on the appropriate set of watchers as
  // determined by the type of event. |original_event| is the original
  // event supplied to OnMouseEvent()/OnTouchEvent(), |pointer_event| is
  // |original_event| converted to a PointerEvent.
  void NotifyWatchers(const ui::PointerEvent& pointer_event,
                      const ui::LocatedEvent& original_event);

  // The true parameter to ObserverList indicates the list must be empty on
  // destruction. Two sets of observers are maintained, one for observers not
  // needing moves |non_move_watchers_| and |move_watchers_| for those
  // observers wanting moves too.
  base::ObserverList<views::PointerWatcher, true>::Unchecked non_move_watchers_;
  base::ObserverList<views::PointerWatcher, true>::Unchecked move_watchers_;
  base::ObserverList<views::PointerWatcher, true>::Unchecked drag_watchers_;

  DISALLOW_COPY_AND_ASSIGN(PointerWatcherAdapter);
};

}  // namespace ash

#endif  // ASH_POINTER_WATCHER_ADAPTER_H_
