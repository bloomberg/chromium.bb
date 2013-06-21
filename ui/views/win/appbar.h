// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_APPBAR_H_
#define UI_VIEWS_WIN_APPBAR_H_

#include <map>

#include <windows.h>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace views {

// Appbar provides an API to query for the edges of the monitor that have an
// autohide bar.
// NOTE: querying is done on a separate thread as it spawns a nested message
// loop. The nested message loop is particularly problematic here as it's
// possible for the nested message loop to run during window creation and
// startup time (WM_NCCALCSIZE is called at creation time).
class Appbar {
 public:
  enum Edge {
    EDGE_TOP    = 1 << 0,
    EDGE_LEFT   = 1 << 1,
    EDGE_BOTTOM = 1 << 2,
    EDGE_RIGHT  = 1 << 3,
  };

  // Returns the singleton instance.
  static Appbar* instance();

  // Starts a query for the autohide edges of the specified monitor and returns
  // the current value. If the edges have changed |callback| is subsequently
  // invoked. If the edges have not changed |callback| is never run.
  //
  // Return value is a bitmask of Edges.
  int GetAutohideEdges(HMONITOR monitor, const base::Closure& callback);

 private:
  typedef std::map<HMONITOR, int> EdgeMap;

  Appbar();
  ~Appbar();

  // Callback on main thread with the edges. |returned_edges| is the value that
  // was returned from the call to GetAutohideEdges() that initiated the lookup.
  void OnGotEdges(const base::Closure& callback,
                  HMONITOR monitor,
                  int returned_edges,
                  int* edges);

  EdgeMap edge_map_;

  base::WeakPtrFactory<Appbar> weak_factory_;

  // If true we're in the process of notifying a callback. When true we do not
  // start a new query.
  bool in_callback_;

  DISALLOW_COPY_AND_ASSIGN(Appbar);
};

}  // namespace views

#endif  // UI_VIEWS_WIN_APPBAR_H_

