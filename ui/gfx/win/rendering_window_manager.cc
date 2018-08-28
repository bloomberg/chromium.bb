// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/rendering_window_manager.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"

namespace gfx {

// static
RenderingWindowManager* RenderingWindowManager::GetInstance() {
  static base::NoDestructor<RenderingWindowManager> instance;
  return instance.get();
}

void RenderingWindowManager::RegisterParent(HWND parent) {
  base::AutoLock lock(lock_);
  registered_hwnds_.emplace(parent, nullptr);
}

bool RenderingWindowManager::RegisterChild(HWND parent, HWND child) {
  if (!child)
    return false;

  base::AutoLock lock(lock_);

  // If |parent| wasn't registered or there is already a child window registered
  // for |parent| don't do anything.
  auto it = registered_hwnds_.find(parent);
  if (it == registered_hwnds_.end() || it->second)
    return false;

  it->second = child;

  // Call ::SetParent() from a worker thread instead of the IO thread. The
  // ::SetParent() call could block the IO thread waiting on the UI thread and
  // deadlock.
  auto callback = base::BindOnce(
      [](HWND parent, HWND child) {
        ::SetParent(child, parent);
        // Move D3D window behind Chrome's window to avoid losing some messages.
        ::SetWindowPos(child, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      },
      parent, child);
  base::PostTaskWithTraits(FROM_HERE, {base::TaskPriority::USER_BLOCKING},
                           std::move(callback));

  return true;
}

void RenderingWindowManager::UnregisterParent(HWND parent) {
  base::AutoLock lock(lock_);
  registered_hwnds_.erase(parent);
}

bool RenderingWindowManager::HasValidChildWindow(HWND parent) {
  base::AutoLock lock(lock_);
  auto it = registered_hwnds_.find(parent);
  if (it == registered_hwnds_.end())
    return false;
  return !!it->second && ::IsWindow(it->second);
}

RenderingWindowManager::RenderingWindowManager() {}
RenderingWindowManager::~RenderingWindowManager() {}

}  // namespace gfx
