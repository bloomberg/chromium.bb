// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_MANAGER_H_

#include <map>

#include "ui/gfx/native_widget_types.h"

namespace ui {

class DriWindowDelegate;

class DriWindowManager {
 public:
  DriWindowManager();
  ~DriWindowManager();

  gfx::AcceleratedWidget NextAcceleratedWidget();

  // Adds a delegate for |widget|. Note: |widget| should not be associated with
  // a delegate when calling this function.
  void AddWindowDelegate(gfx::AcceleratedWidget widget,
                         DriWindowDelegate* surface);

  // Removes the delegate for |widget|. Note: |widget| must have a delegate
  // associated with it when calling this function.
  void RemoveWindowDelegate(gfx::AcceleratedWidget widget);

  // Returns the delegate associated with |widget|. Note: This function should
  // be called only if a valid delegate has been associated with |widget|.
  DriWindowDelegate* GetWindowDelegate(gfx::AcceleratedWidget widget);

  // Check if |widget| has a valid delegate associated with it.
  bool HasWindowDelegate(gfx::AcceleratedWidget widget);

 private:
  typedef std::map<gfx::AcceleratedWidget, DriWindowDelegate*>
      WidgetToDelegateMap;

  WidgetToDelegateMap delegate_map_;
  gfx::AcceleratedWidget last_allocated_widget_;

  DISALLOW_COPY_AND_ASSIGN(DriWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_MANAGER_H_
