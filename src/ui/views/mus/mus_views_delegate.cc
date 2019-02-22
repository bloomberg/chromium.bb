// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_views_delegate.h"

#include "ui/views/mus/ax_remote_host.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

namespace views {

MusViewsDelegate::MusViewsDelegate() = default;

MusViewsDelegate::~MusViewsDelegate() = default;

void MusViewsDelegate::NotifyAccessibilityEvent(View* view,
                                                ax::mojom::Event event_type) {
  if (MusClient::Get()->ax_remote_host())
    MusClient::Get()->ax_remote_host()->HandleEvent(view, event_type);
}

void MusViewsDelegate::AddPointerWatcher(PointerWatcher* pointer_watcher,
                                         bool wants_moves) {
  MusClient::Get()->pointer_watcher_event_router()->AddPointerWatcher(
      pointer_watcher, wants_moves);
}

void MusViewsDelegate::RemovePointerWatcher(PointerWatcher* pointer_watcher) {
  MusClient::Get()->pointer_watcher_event_router()->RemovePointerWatcher(
      pointer_watcher);
}

bool MusViewsDelegate::IsPointerWatcherSupported() const {
  return true;
}

}  // namespace views
