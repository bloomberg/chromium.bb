// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_backend_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

AppCacheBackendImpl::AppCacheBackendImpl(AppCacheServiceImpl* service,
                                         int process_id,
                                         int routing_id)
    : service_(service),
      process_id_(process_id),
      routing_id_(routing_id),
      security_policy_handle_(
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              process_id)) {
  DCHECK(service);
  DCHECK(security_policy_handle_.is_valid());

  if (!security_policy_handle_.is_valid())
    base::debug::DumpWithoutCrashing();
}

AppCacheBackendImpl::~AppCacheBackendImpl() = default;

void AppCacheBackendImpl::RegisterHost(
    mojo::PendingReceiver<blink::mojom::AppCacheHost> host_receiver,
    mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend_remote,
    const base::UnguessableToken& host_id) {
  service_->RegisterHost(std::move(host_receiver), std::move(frontend_remote),
                         host_id, routing_id_, process_id_,
                         security_policy_handle_.Duplicate(),
                         mojo::GetBadMessageCallback());
}

}  // namespace content
