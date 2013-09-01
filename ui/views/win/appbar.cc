// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/appbar.h"

#include <shellapi.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "base/win/scoped_com_initializer.h"
#include "ui/views/widget/monitor_win.h"

namespace views {

namespace {

void GetEdgesOnWorkerThread(HMONITOR monitor, int* edge) {
  base::win::ScopedCOMInitializer com_initializer;
  *edge = 0;
  if (GetTopmostAutoHideTaskbarForEdge(ABE_LEFT, monitor))
    *edge = Appbar::EDGE_LEFT;
  if (GetTopmostAutoHideTaskbarForEdge(ABE_TOP, monitor))
    *edge = Appbar::EDGE_TOP;
  if (GetTopmostAutoHideTaskbarForEdge(ABE_RIGHT, monitor))
    *edge = Appbar::EDGE_RIGHT;
  if (GetTopmostAutoHideTaskbarForEdge(ABE_BOTTOM, monitor))
    *edge = Appbar::EDGE_BOTTOM;
}

}

// static
Appbar* Appbar::instance() {
  static Appbar* appbar = NULL;
  if (!appbar)
    appbar = new Appbar();
  return appbar;
}

int Appbar::GetAutohideEdges(HMONITOR monitor, const base::Closure& callback) {
  // Initialize the map with EDGE_BOTTOM. This is important, as if we return an
  // initial value of 0 (no auto-hide edges) then we'll go fullscreen and
  // windows will automatically remove WS_EX_TOPMOST from the appbar resulting
  // in us thinking there is no auto-hide edges. By returning at least one edge
  // we don't initially go fullscreen until we figure out the real auto-hide
  // edges.
  if (edge_map_.find(monitor) == edge_map_.end())
    edge_map_[monitor] = Appbar::EDGE_BOTTOM;
  if (!in_callback_) {
    int* edge = new int;
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&GetEdgesOnWorkerThread,
                   monitor,
                   base::Unretained(edge)),
        base::Bind(&Appbar::OnGotEdges,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   monitor,
                   edge_map_[monitor],
                   base::Owned(edge)),
        false);
  }
  return edge_map_[monitor];
}

Appbar::Appbar() : weak_factory_(this), in_callback_(false) {
}

Appbar::~Appbar() {
}

void Appbar::OnGotEdges(const base::Closure& callback,
                        HMONITOR monitor,
                        int returned_edges,
                        int* edges) {
  edge_map_[monitor] = *edges;
  if (returned_edges == *edges)
    return;

  base::AutoReset<bool> in_callback_setter(&in_callback_, true);
  callback.Run();
}

}  // namespace views
