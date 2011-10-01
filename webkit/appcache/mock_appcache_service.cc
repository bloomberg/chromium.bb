// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/mock_appcache_service.h"

#include "base/message_loop.h"

namespace appcache {

static void DeferredCallCallback(net::OldCompletionCallback* callback, int rv) {
  callback->Run(rv);
}

void MockAppCacheService::DeleteAppCachesForOrigin(
    const GURL& origin, net::OldCompletionCallback* callback) {
  ++delete_called_count_;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableFunction(&DeferredCallCallback, callback,
                          mock_delete_appcaches_for_origin_result_));
}

}  // namespace appcache
