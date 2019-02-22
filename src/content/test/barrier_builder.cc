// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/barrier_builder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace content {

// The callbacks returned by |AddClosure| can get called after destruction of
// BarrierBuilder, so there needs to be an internal class to hold the final
// callback.
class BarrierBuilder::InternalBarrierBuilder
    : public base::RefCountedThreadSafe<
          BarrierBuilder::InternalBarrierBuilder> {
 public:
  InternalBarrierBuilder(base::OnceClosure done_closure)
      : done_runner_(std::move(done_closure)) {}

 private:
  friend class base::RefCountedThreadSafe<
      BarrierBuilder::InternalBarrierBuilder>;

  ~InternalBarrierBuilder() = default;

  base::ScopedClosureRunner done_runner_;

  DISALLOW_COPY_AND_ASSIGN(InternalBarrierBuilder);
};

BarrierBuilder::BarrierBuilder(base::OnceClosure done_closure)
    : internal_barrier_(
          new BarrierBuilder::InternalBarrierBuilder(std::move(done_closure))) {
}

BarrierBuilder::~BarrierBuilder() = default;

base::OnceClosure BarrierBuilder::AddClosure() {
  return base::BindOnce(
      [](scoped_refptr<BarrierBuilder::InternalBarrierBuilder>) {},
      internal_barrier_);
}

}  // namespace content
