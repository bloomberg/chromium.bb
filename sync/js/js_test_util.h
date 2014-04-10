// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JS_JS_TEST_UTIL_H_
#define SYNC_JS_JS_TEST_UTIL_H_

#include <ostream>
#include <string>

#include "base/memory/weak_ptr.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_backend.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace syncer {

class JsEventDetails;

// Defined for googletest.  Equivalent to "*os << args.ToString()".
void PrintTo(const JsEventDetails& details, ::std::ostream* os);

// A gmock matcher for JsEventDetails.  Use like:
//
//   EXPECT_CALL(mock, HandleJsEvent("foo", HasArgs(expected_details)));
::testing::Matcher<const JsEventDetails&> HasDetails(
    const JsEventDetails& expected_details);

// Like HasDetails() but takes a DictionaryValue instead.
::testing::Matcher<const JsEventDetails&> HasDetailsAsDictionary(
    const base::DictionaryValue& expected_details);

// Mocks.

class MockJsBackend : public JsBackend,
                      public base::SupportsWeakPtr<MockJsBackend> {
 public:
  MockJsBackend();
  virtual ~MockJsBackend();

  WeakHandle<JsBackend> AsWeakHandle();

  MOCK_METHOD1(SetJsEventHandler, void(const WeakHandle<JsEventHandler>&));
};

class MockJsController : public JsController,
                         public base::SupportsWeakPtr<MockJsController> {
 public:
  MockJsController();
  virtual ~MockJsController();

  MOCK_METHOD1(AddJsEventHandler, void(JsEventHandler*));
  MOCK_METHOD1(RemoveJsEventHandler, void(JsEventHandler*));
};

class MockJsEventHandler
    : public JsEventHandler,
      public base::SupportsWeakPtr<MockJsEventHandler> {
 public:
  MockJsEventHandler();
  virtual ~MockJsEventHandler();

  WeakHandle<JsEventHandler> AsWeakHandle();

  MOCK_METHOD2(HandleJsEvent,
               void(const ::std::string&, const JsEventDetails&));
};

}  // namespace syncer

#endif  // SYNC_JS_JS_TEST_UTIL_H_
