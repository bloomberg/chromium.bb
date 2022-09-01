// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_

#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace quiche {

inline void QuicheRunSystemEventLoopIterationImpl() {
  base::RunLoop().RunUntilIdle();
}

class QuicheSystemEventLoopImpl {
 public:
  explicit QuicheSystemEventLoopImpl(std::string context_name) {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams(context_name);
  }

 private:
  base::SingleThreadTaskExecutor io_task_executor_{base::MessagePumpType::IO};
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_SYSTEM_EVENT_LOOP_IMPL_H_
