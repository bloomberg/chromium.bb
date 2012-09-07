// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/non_blocking_invalidator.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "google/cacheinvalidation/types.pb.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/invalidator_test_template.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Needed by WaitForInvalidator().
void DoNothing() {}

class NonBlockingInvalidatorTestDelegate {
 public:
  NonBlockingInvalidatorTestDelegate() : io_thread_("IO thread") {}

  ~NonBlockingInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& initial_state,
      const base::WeakPtr<InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
    request_context_getter_ =
        new TestURLRequestContextGetter(io_thread_.message_loop_proxy());
    notifier::NotifierOptions invalidator_options;
    invalidator_options.request_context_getter = request_context_getter_;
    invalidator_.reset(
        new NonBlockingInvalidator(
            invalidator_options,
            InvalidationVersionMap(),
            initial_state,
            MakeWeakHandle(invalidation_state_tracker),
            "fake_client_info"));
  }

  Invalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    invalidator_.reset();
    request_context_getter_ = NULL;
    io_thread_.Stop();
    message_loop_.RunAllPending();
  }

  void WaitForInvalidator() {
    base::RunLoop run_loop;
    ASSERT_TRUE(
        io_thread_.message_loop_proxy()->PostTaskAndReply(
            FROM_HERE,
            base::Bind(&DoNothing),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TriggerOnInvalidatorStateChange(InvalidatorState state) {
    invalidator_->OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(const ObjectIdStateMap& id_state_map,
                                     IncomingInvalidationSource source) {
    invalidator_->OnIncomingInvalidation(id_state_map, source);
  }

  static bool InvalidatorHandlesDeprecatedState() {
    return true;
  }

 private:
  MessageLoop message_loop_;
  base::Thread io_thread_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<NonBlockingInvalidator> invalidator_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    NonBlockingInvalidatorTest, InvalidatorTest,
    NonBlockingInvalidatorTestDelegate);

}  // namespace

}  // namespace syncer
