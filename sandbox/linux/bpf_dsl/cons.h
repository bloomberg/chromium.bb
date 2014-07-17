// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_CONS_H_
#define SANDBOX_LINUX_BPF_DSL_CONS_H_

#include "base/memory/ref_counted.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// Cons provides an immutable linked list abstraction as commonly
// provided in functional programming languages like Lisp or Haskell.
template <typename T>
class Cons : public base::RefCounted<Cons<T> > {
 public:
  // List provides an abstraction for referencing a list of zero or
  // more Cons nodes.
  typedef scoped_refptr<const Cons<T> > List;

  // Return this node's head element.
  const T& head() const { return head_; }

  // Return this node's tail element.
  List tail() const { return tail_; }

  // Construct a new List using |head| and |tail|.
  static List Make(const T& head, List tail) {
    return make_scoped_refptr(new const Cons<T>(head, tail));
  }

 private:
  Cons(const T& head, List tail) : head_(head), tail_(tail) {}
  virtual ~Cons() {}

  T head_;
  List tail_;

  friend class base::RefCounted<Cons<T> >;
  DISALLOW_COPY_AND_ASSIGN(Cons);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_CONS_H_
