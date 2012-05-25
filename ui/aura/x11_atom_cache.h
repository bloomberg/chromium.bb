// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_X11_ATOM_CACHE_H_
#define UI_AURA_X11_ATOM_CACHE_H_

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"

#include <X11/Xlib.h>

#include <map>
#include <string>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

namespace aura {

// Pre-caches all Atoms on first use to minimize roundtrips to the X11
// server. Assumes that we only have a single X11 display,
// base::MessagePumpX::GetDefaultXDisplay().
class AURA_EXPORT X11AtomCache {
 public:
  // Preinterns the NULL terminated list of string |to_cache_ on |xdisplay|.
  X11AtomCache(Display* xdisplay, const char** to_cache);
  ~X11AtomCache();

  // Returns the pre-interned Atom without having to go to the x server.
  ::Atom GetAtom(const char*) const;

 private:
  Display* xdisplay_;

  std::map<std::string, ::Atom> cached_atoms_;

  DISALLOW_COPY_AND_ASSIGN(X11AtomCache);
};

}  // namespace aura

#endif  // UI_AURA_ATOM_CACHE_H_
