// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/deps_iterator.h"

#include "tools/gn/target.h"

DepsIterator::DepsIterator(const Target* t) : current_index_(0) {
  vect_stack_[0] = &t->public_deps();
  vect_stack_[1] = &t->private_deps();
  vect_stack_[2] = &t->data_deps();

  if (vect_stack_[0]->empty())
    Advance();
}

// Iterate over the public and private linked deps, but not the data deps.
DepsIterator::DepsIterator(const Target* t, LinkedOnly) : current_index_(0) {
  vect_stack_[0] = &t->public_deps();
  vect_stack_[1] = &t->private_deps();
  vect_stack_[2] = NULL;

  if (vect_stack_[0]->empty())
    Advance();
}

// Advance to the next position. This assumes there are more vectors.
//
// For internal use, this function tolerates an initial index equal to the
// length of the current vector. In this case, it will advance to the next
// one.
void DepsIterator::Advance() {
  DCHECK(vect_stack_[0]);

  current_index_++;
  if (current_index_ >= vect_stack_[0]->size()) {
    // Advance to next vect. Shift the elements left by one.
    vect_stack_[0] = vect_stack_[1];
    vect_stack_[1] = vect_stack_[2];
    vect_stack_[2] = NULL;

    current_index_ = 0;

    if (vect_stack_[0] && vect_stack_[0]->empty())
      Advance();
  }
}
