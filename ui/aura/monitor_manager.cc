// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor_manager.h"

#include "base/basictypes.h"
#include "base/stl_util.h"
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace {

// A monitor manager assuming there is one monitor.
class SingleMonitorManager : public MonitorManager,
                             public WindowObserver {
 public:
  SingleMonitorManager(RootWindow* root_window)
      : root_window_(root_window),
        monitor_(new Monitor()) {
    root_window_->AddObserver(this);
    Update(root_window_->bounds().size());
  }

  virtual ~SingleMonitorManager() {
    if (root_window_)
      root_window_->RemoveObserver(this);
  }

  // MonitorManager overrides:
  virtual const Monitor* GetMonitorNearestWindow(const Window* window) const {
    return monitor_.get();
  }
  virtual const Monitor* GetMonitorNearestPoint(const gfx::Point& point) const {
    return monitor_.get();
  }
  virtual const Monitor* GetPrimaryMonitor() const {
    return monitor_.get();
  }
  virtual size_t GetNumMonitors() const {
    return 1;
  }
  virtual Monitor* GetMonitorNearestWindow(const Window* window) {
    return monitor_.get();
  }

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(Window* window, const gfx::Rect& bounds)
      OVERRIDE {
    Update(bounds.size());
  }
  virtual void OnWindowDestroying(Window* window) {
    if (root_window_ == window)
      root_window_ = NULL;
  }

 private:
  void Update(const gfx::Size size) {
    gfx::Rect new_bounds(size);
    monitor_->set_bounds(new_bounds);
  }

  RootWindow* root_window_;
  scoped_ptr<Monitor> monitor_;

  DISALLOW_COPY_AND_ASSIGN(SingleMonitorManager);
};

}  // namespace

MonitorManager::MonitorManager() {
}

MonitorManager::~MonitorManager() {
}

// static
MonitorManager* CreateSingleMonitorManager(RootWindow* root_window) {
  return new SingleMonitorManager(root_window);
}

}  // namespace aura
