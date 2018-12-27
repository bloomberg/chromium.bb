// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/delete_soon_with_graph_lock.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

template <typename T>
void DeleteSoonWithGraphLockImpl(const T* object) {
  auto deleter = [](const T* object) {
    // We need to keep the DeferredTaskHandler alive in this scope, as the
    // handler could otherwise hold the last reference to the graph lock.
    DCHECK(IsMainThread());
    scoped_refptr<DeferredTaskHandler> deferred_task_handler =
        &object->GetDeferredTaskHandler();
    DeferredTaskHandler::GraphAutoLocker locker(*deferred_task_handler);
    delete object;
  };

  // Transfer ownership of the doomed object to a unique owner.
  using OwningPtr = std::unique_ptr<const T, decltype(deleter)>;
  OwningPtr owning_ptr(object, deleter);

  // Post a task which will, when run or destroyed, delete the object.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      Thread::MainThread()->GetTaskRunner();
  task_runner->PostNonNestableTask(
      FROM_HERE, WTF::Bind([](OwningPtr) {}, std::move(owning_ptr)));
}

}  // namespace

void DeleteSoonWithGraphLock(const AudioHandler* handler) {
  DeleteSoonWithGraphLockImpl(handler);
}

void DeleteSoonWithGraphLock(const AudioParamHandler* handler) {
  DeleteSoonWithGraphLockImpl(handler);
}

}  // namespace blink
