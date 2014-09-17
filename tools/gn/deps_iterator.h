// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_DEPS_ITERATOR_H_
#define TOOLS_GN_DEPS_ITERATOR_H_

#include "base/basictypes.h"
#include "tools/gn/label_ptr.h"

class Target;

// Iterates over the deps of a target.
//
// Since there are multiple kinds of deps, this iterator allows looping over
// each one in one loop.
class DepsIterator {
 public:
  enum LinkedOnly {
    LINKED_ONLY,
  };

  // Iterate over public, private, and data deps.
  explicit DepsIterator(const Target* t);

  // Iterate over the public and private linked deps, but not the data deps.
  DepsIterator(const Target* t, LinkedOnly);

  // Returns true when there are no more targets.
  bool done() const {
    return !vect_stack_[0];
  }

  // Advance to the next position. This assumes !done().
  //
  // For internal use, this function tolerates an initial index equal to the
  // length of the current vector. In this case, it will advance to the next
  // one.
  void Advance();

  // The current dependency.
  const LabelTargetPair& pair() const {
    DCHECK_LT(current_index_, vect_stack_[0]->size());
    return (*vect_stack_[0])[current_index_];
  }

  // The pointer to the current dependency.
  const Target* target() const { return pair().ptr; }

  // The label of the current dependency.
  const Label& label() const { return pair().label; }

 private:
  const LabelTargetVector* vect_stack_[3];

  size_t current_index_;

  DISALLOW_COPY_AND_ASSIGN(DepsIterator);
};

#endif  // TOOLS_GN_DEPS_ITERATOR_H_
