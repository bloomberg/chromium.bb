// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_APP_THREAD_POOL_INIT_PARAMS_CALLBACK_H_
#define IOS_WEB_PUBLIC_APP_THREAD_POOL_INIT_PARAMS_CALLBACK_H_

#include "base/callback_forward.h"
#include "base/task/thread_pool/thread_pool.h"

namespace web {

// Callback which returns a pointer to InitParams for base::ThreadPool.
typedef base::OnceCallback<std::unique_ptr<base::ThreadPool::InitParams>()>
    ThreadPoolInitParamsCallback;

}  // namespace web

#endif  // IOS_WEB_PUBLIC_APP_THREAD_POOL_INIT_PARAMS_CALLBACK_H_
