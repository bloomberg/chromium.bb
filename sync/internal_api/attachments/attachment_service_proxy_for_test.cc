// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"

#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"

namespace syncer {

AttachmentServiceProxyForTest::OwningCore::OwningCore(
    scoped_ptr<AttachmentService> wrapped,
    scoped_ptr<base::WeakPtrFactory<AttachmentService> > weak_ptr_factory)
    : Core(weak_ptr_factory->GetWeakPtr()),
      wrapped_(wrapped.Pass()),
      weak_ptr_factory_(weak_ptr_factory.Pass()) {
  DCHECK(wrapped_);
}

AttachmentServiceProxyForTest::OwningCore::~OwningCore() {
}

// Static.
AttachmentServiceProxy AttachmentServiceProxyForTest::Create() {
  scoped_ptr<AttachmentService> wrapped(AttachmentServiceImpl::CreateForTest());
  // This class's base class, AttachmentServiceProxy, must be initialized with a
  // WeakPtr to an AttachmentService.  Because the base class ctor must be
  // invoked before any of this class's members are initialized, we create the
  // WeakPtrFactory here and pass it to the ctor so that it may initialize its
  // base class and own the WeakPtrFactory.
  //
  // We must pass by scoped_ptr because WeakPtrFactory has no copy constructor.
  scoped_ptr<base::WeakPtrFactory<AttachmentService> > weak_ptr_factory(
      new base::WeakPtrFactory<AttachmentService>(wrapped.get()));

  scoped_refptr<Core> core_for_test(
      new OwningCore(wrapped.Pass(), weak_ptr_factory.Pass()));

  scoped_refptr<base::SequencedTaskRunner> runner(
      base::ThreadTaskRunnerHandle::Get());
  if (!runner) {
    // Dummy runner for tests that don't care about AttachmentServiceProxy.
    DVLOG(1) << "Creating dummy MessageLoop for AttachmentServiceProxy.";
    base::MessageLoop loop;
    // This works because |runner| takes a ref to the proxy.
    runner = loop.message_loop_proxy();
  }
  return AttachmentServiceProxyForTest(runner, core_for_test);
}

AttachmentServiceProxyForTest::~AttachmentServiceProxyForTest() {
}

AttachmentServiceProxyForTest::AttachmentServiceProxyForTest(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const scoped_refptr<Core>& core)
    : AttachmentServiceProxy(wrapped_task_runner, core) {
}

}  // namespace syncer
