// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MOUSE_WATCHER_VIEW_HOST_H_
#define UI_VIEWS_MOUSE_WATCHER_VIEW_HOST_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/mouse_watcher.h"

namespace views {

class View;

class VIEWS_EXPORT MouseWatcherViewHost : public MouseWatcherHost {
 public:
  // Creates a new MouseWatcherViewHost. |hot_zone_insets| is added to the
  // bounds of the view to determine the active zone. For example, if
  // |hot_zone_insets.bottom()| is 10, then the listener is not notified if
  // the y coordinate is between the origin of the view and height of the view
  // plus 10.
  MouseWatcherViewHost(View* view, const gfx::Insets& hot_zone_insets);

  MouseWatcherViewHost(const MouseWatcherViewHost&) = delete;
  MouseWatcherViewHost& operator=(const MouseWatcherViewHost&) = delete;

  ~MouseWatcherViewHost() override;

  // MouseWatcherHost.
  bool Contains(const gfx::Point& screen_point, EventType type) override;

 private:
  bool IsCursorInViewZone(const gfx::Point& screen_point);
  bool IsMouseOverWindow();

  // View we're listening for events over.
  raw_ptr<View> view_;
  // Insets added to the bounds of the view.
  const gfx::Insets hot_zone_insets_;
};

}  // namespace views

#endif  // UI_VIEWS_MOUSE_WATCHER_VIEW_HOST_H_
