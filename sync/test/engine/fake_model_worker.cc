// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/fake_model_worker.h"

namespace csync {

FakeModelWorker::FakeModelWorker(ModelSafeGroup group) : group_(group) {}

FakeModelWorker::~FakeModelWorker() {
  // We may need to relax this is FakeModelWorker is used in a
  // multi-threaded test; since ModelSafeWorkers are
  // RefCountedThreadSafe, they could theoretically be destroyed from
  // a different thread.
  DCHECK(CalledOnValidThread());
}

SyncerError FakeModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  DCHECK(CalledOnValidThread());
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup FakeModelWorker::GetModelSafeGroup() {
  DCHECK(CalledOnValidThread());
  return group_;
}

}  // namespace csync
