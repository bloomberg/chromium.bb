// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include <set>

#include "base/logging.h"
#include "ui/base/x/x11_atom_cache.h"

namespace ui {

std::vector< ::Atom> GetTextAtomsFrom(const X11AtomCache* atom_cache) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache->GetAtom("UTF8_STRING"));
  atoms.push_back(atom_cache->GetAtom("STRING"));
  atoms.push_back(atom_cache->GetAtom("TEXT"));
  return atoms;
}

///////////////////////////////////////////////////////////////////////////////

SelectionFormatMap::SelectionFormatMap() {}

SelectionFormatMap::~SelectionFormatMap() {
  // WriteText() inserts the same pointer multiple times for different
  // representations; we need to dedupe it.
  std::set<char*> to_delete;
  for (InternalMap::iterator it = data_.begin(); it != data_.end(); ++it)
    to_delete.insert(it->second.first);

  for (std::set<char*>::iterator it = to_delete.begin(); it != to_delete.end();
       ++it) {
    delete [] *it;
  }
}

void SelectionFormatMap::Insert(::Atom atom, char* data, size_t size) {
  DCHECK(data_.find(atom) == data_.end());
  data_.insert(std::make_pair(atom, std::make_pair(data, size)));
}

}  // namespace ui
