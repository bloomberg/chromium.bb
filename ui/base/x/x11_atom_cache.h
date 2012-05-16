// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_ATOM_CACHE_H_
#define UI_BASE_X_X11_ATOM_CACHE_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "ui/base/ui_export.h"

#include <X11/Xlib.h>

#include <map>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

namespace ui {

// Names of cached atoms that we fetch from X11AtomCache. Adding an entry here
// also requires adding an entry in the cc file.
enum AtomName {
  ATOM_WM_DELETE_WINDOW = 0,
  ATOM__NET_WM_MOVERESIZE,
  ATOM__NET_WM_PING,
  ATOM__NET_WM_PID,
  ATOM_WM_S0,
  ATOM__MOTIF_WM_HINTS,

  ATOM_COUNT
};

// Pre-caches all Atoms on first use to minimize roundtrips to the X11
// server. Assumes that we only have a single X11 display,
// base::MessagePumpX::GetDefaultXDisplay().
class UI_EXPORT X11AtomCache {
 public:
  static X11AtomCache* GetInstance();

  // Returns the pre-interned Atom by enum instead of string.
  ::Atom GetAtom(AtomName name) const;

 private:
  friend struct DefaultSingletonTraits<X11AtomCache>;

  // Constructor performs all interning
  X11AtomCache();
  ~X11AtomCache();

  std::map<AtomName, ::Atom> cached_atoms_;

  DISALLOW_COPY_AND_ASSIGN(X11AtomCache);
};

}  // namespace ui

#endif  // UI_BASE_X_ATOM_CACHE_H_
