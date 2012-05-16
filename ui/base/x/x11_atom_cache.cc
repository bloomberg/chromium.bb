// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_atom_cache.h"

#include <X11/Xatom.h>

#include "base/message_pump_x.h"

namespace ui {

namespace {

// A list of atoms that we'll intern on host creation to save roundtrips to the
// X11 server. Must be kept in sync with AtomCache::AtomName
struct AtomInfo {
  ui::AtomName id;
  const char* name;
} const kAtomList[] = {
  { ATOM_WM_DELETE_WINDOW, "WM_DELETE_WINDOW" },
  { ATOM__NET_WM_MOVERESIZE, "_NET_WM_MOVERESIZE" },
  { ATOM__NET_WM_PING, "_NET_WM_PING" },
  { ATOM__NET_WM_PID, "_NET_WM_PID" },
  { ATOM_WM_S0, "WM_S0" },
  { ATOM__MOTIF_WM_HINTS, "_MOTIF_WM_HINTS" }
};

// Our lists need to stay in sync here.
COMPILE_ASSERT(arraysize(kAtomList) == ui::ATOM_COUNT,
               atom_lists_are_same_size);

}  // namespace

X11AtomCache* X11AtomCache::GetInstance() {
  return Singleton<X11AtomCache>::get();
}

::Atom X11AtomCache::GetAtom(AtomName name) const {
  std::map<AtomName, ::Atom>::const_iterator it = cached_atoms_.find(name);
  DCHECK(it != cached_atoms_.end());
  return it->second;
}

X11AtomCache::X11AtomCache() {
  const char* all_names[ATOM_COUNT];
  ::Atom cached_atoms[ATOM_COUNT];

  for (int i = 0; i < ATOM_COUNT; ++i)
    all_names[i] = kAtomList[i].name;

  // Grab all the atoms we need now to minimize roundtrips to the X11 server.
  XInternAtoms(base::MessagePumpX::GetDefaultXDisplay(),
               const_cast<char**>(all_names), ATOM_COUNT, False,
               cached_atoms);

  for (int i = 0; i < ATOM_COUNT; ++i)
    cached_atoms_.insert(std::make_pair(kAtomList[i].id, cached_atoms[i]));
}

X11AtomCache::~X11AtomCache() {}

}  // namespace ui


