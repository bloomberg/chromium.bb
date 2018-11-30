// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_TEST_COMMON_H_
#define WEBRUNNER_BROWSER_TEST_COMMON_H_

#include "base/optional.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

// Defines mock methods used by tests to observe NavigationStateChangeEvents
// and lower-level WebContentsObserver events.
class MockNavigationObserver : public chromium::web::NavigationEventObserver,
                               public content::WebContentsObserver {
 public:
  using content::WebContentsObserver::Observe;

  MockNavigationObserver();
  ~MockNavigationObserver() override;

  // Acknowledges processing of the most recent OnNavigationStateChanged call.
  void Acknowledge();

  MOCK_METHOD1(MockableOnNavigationStateChanged,
               void(chromium::web::NavigationEvent change));

  // chromium::web::NavigationEventObserver implementation.
  // Proxies calls to MockableOnNavigationStateChanged(), because GMock does
  // not work well with fit::Callbacks inside mocked actions.
  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override;

  // WebContentsObserver implementation.
  MOCK_METHOD2(DidFinishLoad,
               void(content::RenderFrameHost* render_frame_host,
                    const GURL& validated_url));

 private:
  OnNavigationStateChangedCallback navigation_ack_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockNavigationObserver);
};

// Stores an asynchronously generated value for later retrieval, optionally
// invoking a callback on value receipt for controlling test flow.
//
// The value can be read by using the dereference (*) or arrow (->) operators.
// Values must first be received before they can be accessed. Dereferencing a
// value before it is set will produce a CHECK violation.
template <typename T>
class Promise {
 public:
  explicit Promise(base::RepeatingClosure on_capture = base::DoNothing())
      : on_capture_(std::move(on_capture)) {}
  ~Promise() = default;

  // Returns a fit::function<> which will receive and store a value T.
  fit::function<void(T)> GetReceiveCallback() {
    return [this](T value) {
      captured_ = std::move(value);
      on_capture_.Run();
    };
  }

  bool has_value() const { return captured_.has_value(); };

  T& operator*() {
    CHECK(captured_.has_value());
    return *captured_;
  }

  T* operator->() {
    CHECK(captured_.has_value());
    return &*captured_;
  }

 private:
  base::Optional<T> captured_;
  base::RepeatingClosure on_capture_;

  DISALLOW_COPY_AND_ASSIGN(Promise<T>);
};

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_TEST_COMMON_H_
