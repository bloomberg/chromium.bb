// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/overlays/overlay_request_queue.h"
#include "ios/chrome/browser/overlays/overlay_request_queue_impl_observer.h"

// Mutable implementation of OverlayRequestQueue.
class OverlayRequestQueueImpl : public OverlayRequestQueue {
 public:
  ~OverlayRequestQueueImpl() override;

  // Creates an OverlayRequestQueueImpl for |web_state| if one is not created
  // already.  The created instance will be stored under the user data key
  // defined by RequestQueue, allowing access to the pulic interface.
  static void CreateForWebState(web::WebState* web_state);

  // Returns the OverlayRequestQueue stored under OverlayRequestQueue's user
  // data key cast as an OverlayRequestQueueImpl.
  static OverlayRequestQueueImpl* FromWebState(web::WebState* web_state);

  // Adds and removes observers.
  void AddObserver(OverlayRequestQueueImplObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(OverlayRequestQueueImplObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Adds |request| to the queue.
  void AddRequest(std::unique_ptr<OverlayRequest> request);

  // Removes the front-most request from the queue and returns it.  Must be
  // called on a non-empty queue.
  void PopRequest();

 private:
  // WebStateUserData setup.
  friend class web::WebStateUserData<OverlayRequestQueueImpl>;

  // Constructor called by CreateForWebState().
  explicit OverlayRequestQueueImpl();

  // OverlayRequestQueue:
  OverlayRequest* front_request() const override;

  base::ObserverList<OverlayRequestQueueImplObserver>::Unchecked observers_;
  // The queue used to hold the received requests.  Stored as a circular dequeue
  // to allow performant pop events from the front of the queue.
  base::circular_deque<std::unique_ptr<OverlayRequest>> requests_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_QUEUE_IMPL_H_
