// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

// The NoAllocationScope class is used in debug mode to catch unwanted
// allocations. E.g. allocations during GC.
class ThreadState::NoAllocationScope final {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(NoAllocationScope);

 public:
  explicit NoAllocationScope(ThreadState* state) : state_(state) {
    state_->EnterNoAllocationScope();
  }
  ~NoAllocationScope() { state_->LeaveNoAllocationScope(); }

 private:
  ThreadState* const state_;
};

class ThreadState::SweepForbiddenScope final {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(SweepForbiddenScope);

 public:
  explicit SweepForbiddenScope(ThreadState* state) : state_(state) {
    DCHECK(!state_->sweep_forbidden_);
    state_->sweep_forbidden_ = true;
  }
  ~SweepForbiddenScope() {
    DCHECK(state_->sweep_forbidden_);
    state_->sweep_forbidden_ = false;
  }

 private:
  ThreadState* const state_;
};

// Used to denote when access to unmarked objects is allowed but we shouldn't
// resurrect it by making new references (e.g. during weak processing and pre
// finalizer).
class ThreadState::ObjectResurrectionForbiddenScope final {
  STACK_ALLOCATED();
  DISALLOW_COPY_AND_ASSIGN(ObjectResurrectionForbiddenScope);

 public:
  explicit ObjectResurrectionForbiddenScope(ThreadState* state)
      : state_(state) {
    state_->EnterObjectResurrectionForbiddenScope();
  }
  ~ObjectResurrectionForbiddenScope() {
    state_->LeaveObjectResurrectionForbiddenScope();
  }

 private:
  ThreadState* const state_;
};

class ThreadState::MainThreadGCForbiddenScope final {
  STACK_ALLOCATED();

 public:
  MainThreadGCForbiddenScope() : thread_state_(ThreadState::MainThreadState()) {
    thread_state_->EnterGCForbiddenScope();
  }
  ~MainThreadGCForbiddenScope() { thread_state_->LeaveGCForbiddenScope(); }

 private:
  ThreadState* const thread_state_;
};

class ThreadState::GCForbiddenScope final {
  STACK_ALLOCATED();

 public:
  explicit GCForbiddenScope(ThreadState* thread_state)
      : thread_state_(thread_state) {
    thread_state_->EnterGCForbiddenScope();
  }
  ~GCForbiddenScope() { thread_state_->LeaveGCForbiddenScope(); }

 private:
  ThreadState* const thread_state_;
};

// Used to mark when we are in an atomic pause for GC.
class ThreadState::AtomicPauseScope final {
  STACK_ALLOCATED();

 public:
  explicit AtomicPauseScope(ThreadState* thread_state)
      : thread_state_(thread_state), gc_forbidden_scope(thread_state) {
    thread_state_->EnterAtomicPause();
  }
  ~AtomicPauseScope() { thread_state_->LeaveAtomicPause(); }

 private:
  ThreadState* const thread_state_;
  ScriptForbiddenScope script_forbidden_scope;
  GCForbiddenScope gc_forbidden_scope;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREAD_STATE_SCOPES_H_
