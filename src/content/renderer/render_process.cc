// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/renderer/render_process.h"

namespace content {

RenderProcess::RenderProcess(
    const std::string& thread_pool_name,
    std::unique_ptr<base::ThreadPoolInstance::InitParams>
        thread_pool_init_params)
    : ChildProcess(base::ThreadPriority::NORMAL,
                   thread_pool_name,
                   std::move(thread_pool_init_params)) {}

}  // namespace content
