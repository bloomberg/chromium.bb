// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_
#define EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class TestSendMessageFunction;
}

// This class helps us wait for incoming messages sent from javascript via
// chrome.test.sendMessage(). A sample usage would be:
//
//   ExtensionTestMessageListener listener("foo", false);  // won't reply
//   ... do some work
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//
// It is also possible to have the extension wait for our reply. This is
// useful for coordinating multiple pages/processes and having them wait on
// each other. Example:
//
//   ExtensionTestMessageListener listener1("foo1", true);  // will reply
//   ExtensionTestMessageListener listener2("foo2", true);  // will reply
//   ASSERT_TRUE(listener1.WaitUntilSatisfied());
//   ASSERT_TRUE(listener2.WaitUntilSatisfied());
//   ... do some work
//   listener1.Reply("foo2 is ready");
//   listener2.Reply("foo1 is ready");
//
// Further, we can use this to listen for a success and failure message:
//
//   ExtensionTestMessageListener listener("success", will_reply);
//   listener.set_failure_message("failure");
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   if (listener.message() == "success") {
//     HandleSuccess();
//   } else {
//     ASSERT_EQ("failure", listener.message());
//     HandleFailure();
//   }
//
// Or, use it to listen to any arbitrary message:
//
//   ExtensionTestMessageListener listener(will_reply);
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   if (listener.message() == "foo")
//     HandleFoo();
//   else if (listener.message() == "bar")
//     HandleBar();
//   else if (listener.message() == "baz")
//     HandleBaz();
//   else
//     NOTREACHED();
//
// You can also use the class to listen for messages from a specified extension:
//
//   ExtensionTestMessageListener listener(will_reply);
//   listener.set_extension_id(extension->id());
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   ... do some work.
//
// A callback can be set to react to a message from an extension, instead of
// manually waiting.
//
//   ExtensionTestMessageListener listener("do_something", false);
//   listener.SetOnSatisfied(base::BindOnce(&DoSomething));
//   ... run test
//   // SetOnRepeatedlySatisfied could be used if the message is expected
//   // multiple times.
//
// Finally, you can reset the listener to reuse it.
//
//   ExtensionTestMessageListener listener(true);  // will reply
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   while (listener.message() != "end") {
//     Handle(listener.message());
//     listener.Reply("bar");
//     listener.Reset();
//     ASSERT_TRUE(listener.WaitUntilSatisfied());
//   }
//
// Note that when using it in browser tests, you need to make sure it gets
// destructed *before* the browser gets torn down. Two common patterns are to
// either make it a local variable inside your test body, or if it's a member
// variable of a ExtensionBrowserTest subclass, override the
// BrowserTestBase::TearDownOnMainThread() method and clean it up there.
class ExtensionTestMessageListener : public extensions::TestApiObserver {
 public:
  // We immediately start listening for |expected_message|.
  ExtensionTestMessageListener(const std::string& expected_message,
                               bool will_reply);
  // Construct a message listener which will listen for any message.
  explicit ExtensionTestMessageListener(bool will_reply);

  ~ExtensionTestMessageListener() override;

  // This returns true immediately if we've already gotten the expected
  // message, or waits until it arrives. Once this returns true, message() and
  // extension_id_for_message() accessors can be used.
  // Returns false if the wait is interrupted and we still haven't gotten the
  // message, or if the message was equal to |failure_message_|.
  bool WaitUntilSatisfied() WARN_UNUSED_RESULT;

  // Send the given message as a reply. It is only valid to call this after
  // WaitUntilSatisfied has returned true, and if will_reply is true.
  void Reply(const std::string& message);

  // Convenience method that formats int as a string and sends it.
  void Reply(int message);

  void ReplyWithError(const std::string& error);

  // Reset the listener to listen again. No settings (such as messages to
  // listen for) are modified.
  void Reset();

  // Getters and setters.

  bool was_satisfied() const { return satisfied_; }

  void set_failure_message(const std::string& failure_message) {
    failure_message_ = failure_message;
  }

  using OnSatisfiedSignature = void(const std::string&);
  void SetOnSatisfied(base::OnceCallback<OnSatisfiedSignature> on_satisfied) {
    on_satisfied_ = std::move(on_satisfied);
  }
  void SetOnRepeatedlySatisfied(
      base::RepeatingCallback<OnSatisfiedSignature> on_repeatedly_satisfied) {
    on_repeatedly_satisfied_ = on_repeatedly_satisfied;
  }

  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  void set_browser_context(const content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  const std::string& message() const { return message_; }

  const extensions::ExtensionId& extension_id_for_message() const {
    return extension_id_for_message_;
  }

  bool had_user_gesture() const { return had_user_gesture_; }

 private:
  // extensions::TestApiObserver:
  bool OnTestMessage(extensions::TestSendMessageFunction* function,
                     const std::string& message) override;

  // The message we're expecting. If empty, we will wait for any message,
  // regardless of contents.
  const absl::optional<std::string> expected_message_;

  // The last message we received.
  std::string message_;

  // Whether we've seen expected_message_ yet.
  bool satisfied_ = false;

  // Holds the quit Closure for the RunLoop during WaitUntilSatisfied().
  base::OnceClosure quit_wait_closure_;

  // Notifies when the expected message is received.
  base::OnceCallback<OnSatisfiedSignature> on_satisfied_;
  base::RepeatingCallback<OnSatisfiedSignature> on_repeatedly_satisfied_;

  // If true, we expect the calling code to manually send a reply. Otherwise,
  // we send an automatic empty reply to the extension.
  const bool will_reply_;

  // The extension id that we listen for, or empty.
  std::string extension_id_;

  // If non-null, we listen to messages only from this BrowserContext.
  raw_ptr<const content::BrowserContext> browser_context_ = nullptr;

  // The message that signals failure.
  absl::optional<std::string> failure_message_;

  // If we received a message that was the failure message.
  bool failed_ = false;

  // The extension id from which |message_| was received.
  extensions::ExtensionId extension_id_for_message_;

  // Whether the ExtensionFunction handling the message had an active user
  // gesture.
  bool had_user_gesture_ = false;

  // The function we need to reply to.
  scoped_refptr<extensions::TestSendMessageFunction> function_;

  base::ScopedObservation<extensions::TestApiObserverRegistry,
                          extensions::TestApiObserver>
      test_api_observation_{this};
};

#endif  // EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_
