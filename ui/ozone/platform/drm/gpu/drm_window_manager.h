// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_MANAGER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

class DrmWindow;

class OZONE_EXPORT DrmWindowManager {
 public:
  DrmWindowManager();
  ~DrmWindowManager();

  // Adds a delegate for |widget|. Note: |widget| should not be associated with
  // a delegate when calling this function.
  void AddWindowDelegate(gfx::AcceleratedWidget widget,
                         scoped_ptr<DrmWindow> surface);

  // Removes the delegate for |widget|. Note: |widget| must have a delegate
  // associated with it when calling this function.
  scoped_ptr<DrmWindow> RemoveWindowDelegate(gfx::AcceleratedWidget widget);

  // Returns the delegate associated with |widget|. Note: This function should
  // be called only if a valid delegate has been associated with |widget|.
  DrmWindow* GetWindowDelegate(gfx::AcceleratedWidget widget);

 private:
  typedef base::ScopedPtrHashMap<gfx::AcceleratedWidget, DrmWindow>
      WidgetToDelegateMap;

  WidgetToDelegateMap delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(DrmWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_MANAGER_H_
