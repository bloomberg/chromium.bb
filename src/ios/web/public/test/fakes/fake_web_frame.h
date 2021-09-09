// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "ios/web/public/js_messaging/web_frame.h"

namespace web {

// Frame id constants which can be used to initialize FakeWebFrame.
// Frame ids are base16 string of 128 bit numbers.
extern const char kMainFakeFrameId[];
extern const char kInvalidFrameId[];
extern const char kChildFakeFrameId[];
extern const char kChildFakeFrameId2[];

// A fake web frame to use for testing.
class FakeWebFrame : public WebFrame {
 public:
  // Creates a web frame. |frame_id| must be a string representing a valid
  // hexadecimal number.
  static std::unique_ptr<FakeWebFrame> Create(const std::string& frame_id,
                                              bool is_main_frame,
                                              GURL security_origin);

  // Creates a web frame representing the main frame with a frame id of
  // |kMainFakeFrameId|.
  static std::unique_ptr<FakeWebFrame> CreateMainWebFrame(GURL security_origin);

  // Creates a web frame representing the main frame with a frame id of
  // |kChildFakeFrameId|.
  static std::unique_ptr<FakeWebFrame> CreateChildWebFrame(
      GURL security_origin);

  // Returns the most recent JavaScript handler call made to this frame.
  virtual std::string GetLastJavaScriptCall() const = 0;

  // Returns |javascript_calls|. Use LastJavaScriptCall() if possible.
  virtual const std::vector<std::string>& GetJavaScriptCallHistory() = 0;

  // Sets the browser state associated with this frame.
  virtual void set_browser_state(BrowserState* browser_state) = 0;

  // Sets |js_result| that will be passed into callback for |name| function
  // call. The same result will be pass regardless of call arguments.
  // NOTE: The caller is responsible for keeping |js_result| alive for as
  // long as this instance lives.
  virtual void AddJsResultForFunctionCall(base::Value* js_result,
                                          const std::string& function_name) = 0;

  virtual void set_force_timeout(bool force_timeout) = 0;

  // Sets return value |can_call_function_| of CanCallJavaScriptFunction(),
  // which defaults to true.
  virtual void set_can_call_function(bool can_call_function) = 0;

  // Sets a callback to be called at the start of |CallJavaScriptFunction()| if
  // |CanCallJavaScriptFunction()| returns true.
  virtual void set_call_java_script_function_callback(
      base::RepeatingClosure callback) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_FRAME_H_
