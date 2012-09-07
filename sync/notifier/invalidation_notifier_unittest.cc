// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/invalidation_notifier.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidation_state_tracker.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator_test_template.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class InvalidationNotifierTestDelegate {
 public:
  InvalidationNotifierTestDelegate() {}

  ~InvalidationNotifierTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    invalidator_.reset(
        new InvalidationNotifier(
            scoped_ptr<notifier::PushClient>(new notifier::FakePushClient()),
            InvalidationVersionMap(),
            initial_state,
            MakeWeakHandle(invalidation_state_tracker),
            "fake_client_info"));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    // Stopping the invalidation notifier stops its scheduler, which deletes
    // any pending tasks without running them.  Some tasks "run and delete"
    // another task, so they must be run in order to avoid leaking the inner
    // task.  Stopping does not schedule any tasks, so it's both necessary and
    // sufficient to drain the task queue before stopping the notifier.
    message_loop_.RunAllPending();
    invalidator_.reset();
  }

  void WaitForInvalidator() {
    message_loop_.RunAllPending();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(const ObjectIdStateMap& id_state_map,
                                     IncomingInvalidationSource source) {
    // None of the tests actually care about |source|.
    invalidator_->OnInvalidate(id_state_map);
  }

  static bool InvalidatorHandlesDeprecatedState() {
    return true;
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<InvalidationNotifier> invalidator_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    InvalidationNotifierTest, InvalidatorTest,
    InvalidationNotifierTestDelegate);

}  // namespace

}  // namespace syncer
