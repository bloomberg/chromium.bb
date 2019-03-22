// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/bindings_export.h"

namespace mojo {
// Begins issuing temporary pauses on randomly selected Mojo bindings for
// debugging purposes.
MOJO_CPP_BINDINGS_EXPORT void BeginRandomMojoDelays();
namespace internal {
class BindingStateBase;
// Adds a binding state base to make it eligible for pausing.
MOJO_CPP_BINDINGS_EXPORT void MakeBindingRandomlyPaused(
    scoped_refptr<base::SequencedTaskRunner>,
    base::WeakPtr<BindingStateBase>);
}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_TEST_RANDOM_MOJO_DELAYS_H_
