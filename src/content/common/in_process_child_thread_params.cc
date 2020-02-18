// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/in_process_child_thread_params.h"

namespace content {

InProcessChildThreadParams::InProcessChildThreadParams(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    mojo::OutgoingInvitation* mojo_invitation)
    : io_runner_(std::move(io_runner)), mojo_invitation_(mojo_invitation) {}

InProcessChildThreadParams::InProcessChildThreadParams(
    const InProcessChildThreadParams& other) = default;

InProcessChildThreadParams::~InProcessChildThreadParams() {
}

}  // namespace content
