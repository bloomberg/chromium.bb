// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_context.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/quota/quota_manager_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

QuotaContext::QuotaContext(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    storage::GetQuotaSettingsFunc get_settings_function)
    : base::RefCountedDeleteOnSequence<QuotaContext>(
          base::CreateSingleThreadTaskRunner({BrowserThread::IO})),
      io_thread_(base::CreateSingleThreadTaskRunner({BrowserThread::IO})),
      quota_manager_(base::MakeRefCounted<storage::QuotaManager>(
          is_incognito,
          profile_path,
          io_thread_,
          std::move(special_storage_policy),
          std::move(get_settings_function))),
      permission_context_(
          GetContentClient()->browser()->CreateQuotaPermissionContext()) {}

void QuotaContext::BindQuotaManagerHost(
    int process_id,
    int render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&QuotaContext::BindQuotaManagerHostOnIOThread, this,
                     process_id, render_frame_id, origin, std::move(receiver)));
}

QuotaContext::~QuotaContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void QuotaContext::BindQuotaManagerHostOnIOThread(
    int process_id,
    int render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The quota manager currently runs on the I/O thread.
  auto host = std::make_unique<QuotaManagerHost>(process_id, render_frame_id,
                                                 origin, quota_manager_.get(),
                                                 permission_context_.get());
  auto* host_ptr = host.get();
  receivers_.Add(host_ptr, std::move(receiver), std::move(host));
}

void QuotaContext::OverrideQuotaManagerForTesting(
    scoped_refptr<storage::QuotaManager> new_manager) {
  quota_manager_ = std::move(new_manager);
}

}  // namespace content
