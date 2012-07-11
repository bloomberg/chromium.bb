// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_SESSIONS_TEST_SCOPED_SESSION_EVENT_LISTENER_H_
#define SYNC_TEST_SESSIONS_TEST_SCOPED_SESSION_EVENT_LISTENER_H_

#include "sync/sessions/sync_session_context.h"

namespace syncer {
namespace sessions {

// Installs a SyncEventListener to a given session context for the lifetime of
// the TestScopedSessionEventListener.
class TestScopedSessionEventListener {
 public:
  TestScopedSessionEventListener(
      SyncSessionContext* context,
      SyncEngineEventListener* listener)
    : context_(context), listener_(listener) {
      context->listeners_.AddObserver(listener);
  }
  ~TestScopedSessionEventListener() {
    context_->listeners_.RemoveObserver(listener_);
  }
 private:
  SyncSessionContext* context_;
  SyncEngineEventListener* listener_;
  DISALLOW_COPY_AND_ASSIGN(TestScopedSessionEventListener);
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_TEST_SESSIONS_TEST_SCOPED_SESSION_EVENT_LISTENER_H_
