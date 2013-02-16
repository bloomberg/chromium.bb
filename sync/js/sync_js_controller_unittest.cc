// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/js/sync_js_controller.h"

#include "base/message_loop.h"
#include "base/values.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;

class SyncJsControllerTest : public testing::Test {
 protected:
  void PumpLoop() {
    message_loop_.RunUntilIdle();
  }

 private:
  MessageLoop message_loop_;
};

TEST_F(SyncJsControllerTest, Messages) {
  InSequence dummy;
  // |mock_backend| needs to outlive |sync_js_controller|.
  StrictMock<MockJsBackend> mock_backend;
  SyncJsController sync_js_controller;

  base::ListValue arg_list1, arg_list2;
  arg_list1.Append(new base::FundamentalValue(false));
  arg_list2.Append(new base::FundamentalValue(5));
  JsArgList args1(&arg_list1), args2(&arg_list2);

  // TODO(akalin): Write matchers for WeakHandle and use them here
  // instead of _.
  EXPECT_CALL(mock_backend, SetJsEventHandler(_));
  EXPECT_CALL(mock_backend, ProcessJsMessage("test1", HasArgs(args2), _));
  EXPECT_CALL(mock_backend, ProcessJsMessage("test2", HasArgs(args1), _));

  sync_js_controller.AttachJsBackend(mock_backend.AsWeakHandle());
  sync_js_controller.ProcessJsMessage("test1", args2,
                                      WeakHandle<JsReplyHandler>());
  sync_js_controller.ProcessJsMessage("test2", args1,
                                      WeakHandle<JsReplyHandler>());
  PumpLoop();

  // Let destructor of |sync_js_controller| call RemoveBackend().
}

TEST_F(SyncJsControllerTest, QueuedMessages) {
  // |mock_backend| needs to outlive |sync_js_controller|.
  StrictMock<MockJsBackend> mock_backend;
  SyncJsController sync_js_controller;

  base::ListValue arg_list1, arg_list2;
  arg_list1.Append(new base::FundamentalValue(false));
  arg_list2.Append(new base::FundamentalValue(5));
  JsArgList args1(&arg_list1), args2(&arg_list2);

  // Should queue messages.
  sync_js_controller.ProcessJsMessage("test1", args2,
                                      WeakHandle<JsReplyHandler>());
  sync_js_controller.ProcessJsMessage("test2", args1,
                                      WeakHandle<JsReplyHandler>());

  Mock::VerifyAndClearExpectations(&mock_backend);

  // TODO(akalin): Write matchers for WeakHandle and use them here
  // instead of _.
  EXPECT_CALL(mock_backend, SetJsEventHandler(_));
  EXPECT_CALL(mock_backend, ProcessJsMessage("test1", HasArgs(args2), _));
  EXPECT_CALL(mock_backend, ProcessJsMessage("test2", HasArgs(args1), _));

  // Should call the queued messages.
  sync_js_controller.AttachJsBackend(mock_backend.AsWeakHandle());
  PumpLoop();

  // Should do nothing.
  sync_js_controller.AttachJsBackend(WeakHandle<JsBackend>());
  PumpLoop();

  // Should also do nothing.
  sync_js_controller.AttachJsBackend(WeakHandle<JsBackend>());
  PumpLoop();
}

TEST_F(SyncJsControllerTest, Events) {
  InSequence dummy;
  SyncJsController sync_js_controller;

  base::DictionaryValue details_dict1, details_dict2;
  details_dict1.SetString("foo", "bar");
  details_dict2.SetInteger("baz", 5);
  JsEventDetails details1(&details_dict1), details2(&details_dict2);

  StrictMock<MockJsEventHandler> event_handler1, event_handler2;
  EXPECT_CALL(event_handler1, HandleJsEvent("event", HasDetails(details1)));
  EXPECT_CALL(event_handler2, HandleJsEvent("event", HasDetails(details1)));
  EXPECT_CALL(event_handler1,
              HandleJsEvent("anotherevent", HasDetails(details2)));
  EXPECT_CALL(event_handler2,
              HandleJsEvent("anotherevent", HasDetails(details2)));

  sync_js_controller.AddJsEventHandler(&event_handler1);
  sync_js_controller.AddJsEventHandler(&event_handler2);
  sync_js_controller.HandleJsEvent("event", details1);
  sync_js_controller.HandleJsEvent("anotherevent", details2);
  sync_js_controller.RemoveJsEventHandler(&event_handler1);
  sync_js_controller.RemoveJsEventHandler(&event_handler2);
  sync_js_controller.HandleJsEvent("droppedevent", details2);

  PumpLoop();
}

}  // namespace
}  // namespace syncer
