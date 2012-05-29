// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/x11_atom_cache.h"

#include <X11/Xatom.h>

#include "base/message_pump_aurax11.h"
#include "base/memory/scoped_ptr.h"

namespace aura {

X11AtomCache::X11AtomCache(Display* xdisplay, const char** to_cache)
    : xdisplay_(xdisplay) {
  int cache_count = 0;
  for (const char** i = to_cache; *i != NULL; i++)
    cache_count++;

  scoped_array< ::Atom> cached_atoms(new ::Atom[cache_count]);

  // Grab all the atoms we need now to minimize roundtrips to the X11 server.
  XInternAtoms(base::MessagePumpAuraX11::GetDefaultXDisplay(),
               const_cast<char**>(to_cache), cache_count, False,
               cached_atoms.get());

  for (int i = 0; i < cache_count; ++i)
    cached_atoms_.insert(std::make_pair(to_cache[i], cached_atoms[i]));
}

X11AtomCache::~X11AtomCache() {}

::Atom X11AtomCache::GetAtom(const char* name) const {
  std::map<std::string, ::Atom>::const_iterator it = cached_atoms_.find(name);
  CHECK(it != cached_atoms_.end());
  return it->second;
}

}  // namespace aura
