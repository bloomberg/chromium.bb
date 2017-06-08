// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_ATOM_CACHE_H_
#define UI_GFX_X_X11_ATOM_CACHE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/x/x11_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// Pre-caches all Atoms on first use to minimize roundtrips to the X11
// server. By default, GetAtom() will CHECK() that atoms accessed through
// GetAtom() were passed to the constructor, but this behaviour can be changed
// with allow_uncached_atoms().
class GFX_EXPORT X11AtomCache {
 public:
  static X11AtomCache* GetInstance();

  // Returns the pre-interned Atom without having to go to the x server.
  XAtom GetAtom(const char*) const;

 private:
  friend struct base::DefaultSingletonTraits<X11AtomCache>;

  X11AtomCache();
  ~X11AtomCache();

  XDisplay* xdisplay_;

  mutable std::map<std::string, XAtom> cached_atoms_;

  DISALLOW_COPY_AND_ASSIGN(X11AtomCache);
};

}  // namespace ui

#endif  // UI_GFX_X_X11_ATOM_CACHE_H_
