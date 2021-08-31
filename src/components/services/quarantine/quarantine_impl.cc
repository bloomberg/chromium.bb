// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/quarantine_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/services/quarantine/quarantine.h"

namespace quarantine {

namespace {

void ReplyToCallback(scoped_refptr<base::TaskRunner> task_runner,
                     mojom::Quarantine::QuarantineFileCallback callback,
                     QuarantineFileResult result) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace

QuarantineImpl::QuarantineImpl() = default;

QuarantineImpl::QuarantineImpl(
    mojo::PendingReceiver<mojom::Quarantine> receiver)
    : receiver_(this, std::move(receiver)) {}

QuarantineImpl::~QuarantineImpl() = default;

void QuarantineImpl::QuarantineFile(
    const base::FilePath& full_path,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::string& client_guid,
    mojom::Quarantine::QuarantineFileCallback callback) {
#if defined(OS_MAC)
  // On Mac posting to a new task runner to do the potentially blocking
  // quarantine work.
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
#else   // OS_MAC
  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
#endif  // OS_MAC
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &quarantine::QuarantineFile, full_path, source_url, referrer_url,
          client_guid,
          base::BindOnce(&ReplyToCallback, base::ThreadTaskRunnerHandle::Get(),
                         std::move(callback))));
}

}  // namespace quarantine
