// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_UTILS_H_
#define UI_BASE_X_SELECTION_UTILS_H_

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include <map>

#include "base/basictypes.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_export.h"

namespace ui {
class X11AtomCache;

// Returns a list of all text atoms that we handle.
UI_EXPORT std::vector< ::Atom> GetTextAtomsFrom(const X11AtomCache* atom_cache);

///////////////////////////////////////////////////////////////////////////////

// Represents the selection in different data formats. Binary data passed in is
// assumed to be allocated with new char[], and is owned by SelectionFormatMap.
class UI_EXPORT SelectionFormatMap {
 public:
  // Our internal data store, which we only expose through iterators.
  typedef std::map< ::Atom, std::pair<char*, size_t> > InternalMap;
  typedef std::map< ::Atom, std::pair<char*, size_t> >::const_iterator
      const_iterator;

  SelectionFormatMap();
  ~SelectionFormatMap();

  // Adds the selection in the format |atom|. Ownership of |data| is passed to
  // us.
  void Insert(::Atom atom, char* data, size_t size);

  // Pass through to STL map. Only allow non-mutation access.
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }
  const_iterator find(::Atom atom) const { return data_.find(atom); }
  size_t size() const { return data_.size(); }

 private:
  InternalMap data_;

  DISALLOW_COPY_AND_ASSIGN(SelectionFormatMap);
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_UTILS_H_
