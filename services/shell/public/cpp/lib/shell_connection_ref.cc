// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/shell_connection_ref.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"

namespace shell {

class ShellConnectionRefImpl : public ShellConnectionRef {
 public:
  ShellConnectionRefImpl(
      ShellConnectionRefFactory* factory,
      scoped_refptr<base::SingleThreadTaskRunner> shell_client_task_runner)
      : factory_(factory),
        shell_client_task_runner_(shell_client_task_runner) {}
  ~ShellConnectionRefImpl() override {
#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_)
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
#endif

    if (shell_client_task_runner_->BelongsToCurrentThread()) {
      factory_->Release();
    } else {
      shell_client_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnectionRefFactory::Release,
                     base::Unretained(factory_)));
    }
  }

 private:
  // ShellConnectionRef:
  std::unique_ptr<ShellConnectionRef> Clone() override {
    if (shell_client_task_runner_->BelongsToCurrentThread()) {
      factory_->AddRef();
    } else {
      shell_client_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ShellConnectionRefFactory::AddRef,
                     base::Unretained(factory_)));
    }

#ifndef NDEBUG
    // Ensure that this object is used on only one thread at a time, or else
    // there could be races where the object is being reset on one thread and
    // cloned on another.
    if (clone_task_runner_) {
      DCHECK(clone_task_runner_->BelongsToCurrentThread());
    } else {
      clone_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    }
#endif

    return base::WrapUnique(
        new ShellConnectionRefImpl(factory_, shell_client_task_runner_));
  }

  ShellConnectionRefFactory* factory_;
  scoped_refptr<base::SingleThreadTaskRunner> shell_client_task_runner_;

#ifndef NDEBUG
  scoped_refptr<base::SingleThreadTaskRunner> clone_task_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellConnectionRefImpl);
};

ShellConnectionRefFactory::ShellConnectionRefFactory(
    const base::Closure& quit_closure) : quit_closure_(quit_closure) {
  DCHECK(!quit_closure_.is_null());
}

ShellConnectionRefFactory::~ShellConnectionRefFactory() {}

std::unique_ptr<ShellConnectionRef> ShellConnectionRefFactory::CreateRef() {
  AddRef();
  return base::WrapUnique(
      new ShellConnectionRefImpl(this, base::ThreadTaskRunnerHandle::Get()));
}

void ShellConnectionRefFactory::AddRef() {
  ++ref_count_;
}

void ShellConnectionRefFactory::Release() {
  if (!--ref_count_)
    quit_closure_.Run();
}

}  // namespace shell
