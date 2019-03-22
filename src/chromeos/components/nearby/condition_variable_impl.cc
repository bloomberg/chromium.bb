// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/condition_variable_impl.h"

#include "chromeos/components/nearby/lock_base.h"

namespace chromeos {

namespace nearby {

ConditionVariableImpl::ConditionVariableImpl(location::nearby::Lock* lock)
    : lock_(lock),
      notify_has_been_called_(base::WaitableEvent::ResetPolicy::AUTOMATIC) {}

ConditionVariableImpl::~ConditionVariableImpl() = default;

void ConditionVariableImpl::notify() {
  // Signal() will signal a single outstanding call to Wait(), since it's set to
  // ResetPolicy::AUTOMATIC.
  notify_has_been_called_.Signal();
}

location::nearby::Exception::Value ConditionVariableImpl::wait() {
  lock_->unlock();

  // If |lock_| is still held by the current thread here (e.g., due to it being
  // a recursive lock), then no other thread will be able to acquire |lock_| in
  // order to modify whatever condition this ConditionVariable is waiting on.
  // In that case, the following call to WaitableEvent::Wait() will likely block
  // forever.
  DCHECK(!(static_cast<LockBase* const>(lock_)->IsHeldByCurrentThread()));

  notify_has_been_called_.Wait();
  lock_->lock();

  // Because |notify_has_been_called_| is set to ResetPolicy::AUTOMATIC,
  // Signal() will only wake up ONE outstanding call to Wait().
  // ConditionVariable::notify() is expected to notify ALL outstanding threads,
  // but since WaitableEvent does not currently have a public broadcast method,
  // this Signal() will make sure to wake up the next outstanding thread.
  //
  // NOTE: There exists a small inefficiency here where if this is the last
  // waiting thread, then the Signal() will persist until the next call to
  // notify_has_been_called_.Wait(), causing that next call to return
  // immediately. This should be fine since calling ConditionVariable::wait()
  // should be done within a while-loop anyways, as in:
  //
  //   lock.lock();
  //   while (!condition_is_true)
  //     condition_variable.wait();
  //   // do stuff while condition is true
  //   lock.unlock();
  //
  // The persisting Signal() should merely cause one extra loop around, properly
  // blocking the second time ConditionVariable::wait() is called.
  notify_has_been_called_.Signal();

  return location::nearby::Exception::NONE;
}

}  // namespace nearby

}  // namespace chromeos
