// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_SERVICE_H_
#define COMPONENTS_NACL_COMMON_NACL_SERVICE_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service.h"

std::unique_ptr<service_manager::Service> CreateNaClService(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle* ipc_channel);

#endif  // COMPONENTS_NACL_COMMON_NACL_SERVICE_H_
