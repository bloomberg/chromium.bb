// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {

class WebState;
class TestWebState;

// Decides the navigation policy for a web state.
class WebStatePolicyDecider {
 public:
  // Specifies a navigation decision. Used as a return value by
  // WebStatePolicyDecider::ShouldAllowRequest(), and used by
  // WebStatePolicyDecider::ShouldAllowResponse() when sending its decision to
  // its callback.
  struct PolicyDecision {
    // A policy decision which allows the navigation.
    static PolicyDecision Allow();

    // A policy decision which cancels the navigation.
    static PolicyDecision Cancel();

    // A policy decision which cancels the navigation and displays |error|.
    // NOTE: The |error| will only be displayed if the associated navigation is
    // being loaded in the main frame.
    static PolicyDecision CancelAndDisplayError(NSError* error);

    // Whether or not the navigation will continue.
    bool ShouldAllowNavigation() const;

    // Whether or not the navigation will be cancelled.
    bool ShouldCancelNavigation() const;

    // Whether or not an error should be displayed. Always returns false if
    // |ShouldAllowNavigation| is true.
    // NOTE: Will return true when the receiver is created with
    // |CancelAndDisplayError| even though an error will only end up being
    // displayed if the associated navigation is occurring in the main frame.
    bool ShouldDisplayError() const;

    // The error to display when |ShouldDisplayError| is true.
    NSError* GetDisplayError() const;

   private:
    // The decisions which can be taken for a given navigation.
    enum class Decision {
      // Allow the navigation to proceed.
      kAllow,

      // Cancel the navigation.
      kCancel,

      // Cancel the navigation and display an error.
      kCancelAndDisplayError,
    };

    PolicyDecision(Decision decision, NSError* error)
        : decision(decision), error(error) {}

    // The decision to be taken for a given navigation.
    Decision decision = Decision::kAllow;

    // An error associated with the navigation. This error will be displayed if
    // |decision| is |kCancelAndDisplayError|.
    NSError* error = nil;
  };

  // Callback used to provide asynchronous policy decisions.
  typedef base::OnceCallback<void(PolicyDecision)> PolicyDecisionCallback;

  // Data Transfer Object for the additional information about navigation
  // request passed to WebStatePolicyDecider::ShouldAllowRequest().
  struct RequestInfo {
    RequestInfo(ui::PageTransition transition_type,
                bool target_frame_is_main,
                bool has_user_gesture)
        : transition_type(transition_type),
          target_frame_is_main(target_frame_is_main),
          has_user_gesture(has_user_gesture) {}
    // The navigation page transition type.
    ui::PageTransition transition_type =
        ui::PageTransition::PAGE_TRANSITION_FIRST;
    // Indicates whether the navigation target frame is the main frame.
    bool target_frame_is_main = false;
    // Indicates if there was a recent user interaction with the request frame.
    bool has_user_gesture = false;
  };

  // Removes self as a policy decider of |web_state_|.
  virtual ~WebStatePolicyDecider();

  // Asks the decider whether the navigation corresponding to |request| should
  // be allowed to continue. The first policy decider returning a PolicyDecision
  // where ShouldCancelNavigation() is true will be the PolicyDecision used for
  // the navigation. This means that a policy decider may not be called and have
  // its expected decision performed for a given navigation. As such, the
  // highest priority policy deciders should be added first to ensure those
  // decisions are prioritized.
  // Called before WebStateObserver::DidStartNavigation.
  // Defaults to PolicyDecision::Allow() if not overridden.
  // Never called in the following cases:
  //  - same-document back-forward and state change navigations
  virtual PolicyDecision ShouldAllowRequest(NSURLRequest* request,
                                            const RequestInfo& request_info);

  // Asks the decider whether the navigation corresponding to |response| should
  // be allowed to continue. Defaults to PolicyDecision::Allow() if not
  // overridden. |for_main_frame| indicates whether the frame being navigated is
  // the main frame. Called before WebStateObserver::DidFinishNavigation. Calls
  // |callback| with the decision.
  // Never called in the following cases:
  //  - same-document navigations (unless ititiated via LoadURLWithParams)
  //  - going back after form submission navigation
  //  - user-initiated POST navigation on iOS 10
  virtual void ShouldAllowResponse(NSURLResponse* response,
                                   bool for_main_frame,
                                   PolicyDecisionCallback callback);

  // Notifies the policy decider that the web state is being destroyed.
  // Gives subclasses a chance to cleanup.
  // The policy decider must not be destroyed while in this call, as removing
  // while iterating is not supported.
  virtual void WebStateDestroyed() {}

  WebState* web_state() const { return web_state_; }

 protected:
  // Designated constructor. Subscribes to |web_state|.
  explicit WebStatePolicyDecider(WebState* web_state);

 private:
  friend class WebStateImpl;
  friend class TestWebState;

  // Resets the current web state.
  void ResetWebState();

  // The web state to decide navigation policy for.
  WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(WebStatePolicyDecider);
};
}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
