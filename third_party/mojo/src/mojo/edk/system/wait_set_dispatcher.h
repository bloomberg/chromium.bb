// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_

#include <deque>
#include <map>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/system/macros.h"
#include "third_party/mojo/src/mojo/edk/system/awakable_list.h"
#include "third_party/mojo/src/mojo/edk/system/dispatcher.h"
#include "third_party/mojo/src/mojo/edk/system/mutex.h"
#include "third_party/mojo/src/mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace system {

class MOJO_SYSTEM_IMPL_EXPORT WaitSetDispatcher : public Dispatcher {
 public:
  WaitSetDispatcher();
  ~WaitSetDispatcher() override;

  // |Dispatcher| public methods:
  Type GetType() const override;

 private:
  // Internal implementation of Awakable.
  class Waiter;

  struct WaitState {
    scoped_refptr<Dispatcher> dispatcher;
    MojoHandleSignals signals;
    uintptr_t context;
  };

  // |Dispatcher| protected methods:
  void CloseImplNoLock() override;
  void CancelAllAwakablesNoLock() override;
  MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) override;
  void RemoveAwakableImplNoLock(Awakable* awakable,
                                HandleSignalsState* signals_state) override;
  MojoResult AddWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher,
      MojoHandleSignals signals,
      uintptr_t context) override;
  MojoResult RemoveWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher) override;
  MojoResult GetReadyDispatchersImplNoLock(
      UserPointer<uint32_t> count,
      DispatcherVector* dispatchers,
      UserPointer<MojoResult> results,
      UserPointer<uintptr_t> contexts) override;
  HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;

  // Signal that the dispatcher indexed by |context| has been woken up with
  // |result| and is now ready.
  void WakeDispatcher(MojoResult result, uintptr_t context);

  // Map of dispatchers being waited on. Key is a Dispatcher* casted to a
  // uintptr_t, and should be treated as an opaque value and not casted back.
  std::map<uintptr_t, WaitState> waiting_dispatchers_ MOJO_GUARDED_BY(mutex());

  // Separate mutex that can be locked without locking |mutex()|.
  mutable Mutex awoken_mutex_ MOJO_ACQUIRED_AFTER(mutex());
  // List of dispatchers that have been woken up. Any dispatcher in this queue
  // will NOT currently be waited on.
  std::deque<std::pair<uintptr_t, MojoResult>> awoken_queue_
      MOJO_GUARDED_BY(awoken_mutex_);
  // List of dispatchers that have been woken up and retrieved on the last call
  // to |GetReadyDispatchers()|.
  std::deque<uintptr_t> processed_dispatchers_ MOJO_GUARDED_BY(awoken_mutex_);

  // Separate mutex that can be locked without locking |mutex()|.
  Mutex awakable_mutex_ MOJO_ACQUIRED_AFTER(mutex());
  // List of dispatchers being waited on.
  AwakableList awakable_list_ MOJO_GUARDED_BY(awakable_mutex_);

  // Waiter used to wait on dispatchers.
  scoped_ptr<Waiter> waiter_;
};

}  // namespace system
}  // namespace mojo

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_EDK_SYSTEM_WAIT_SET_DISPATCHER_H_
