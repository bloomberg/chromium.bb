// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_SNAP_FLING_CONTROLLER_H_
#define UI_EVENTS_BLINK_SNAP_FLING_CONTROLLER_H_

#include <memory>

#include "base/time/time.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureEvent;
class WebInputEvent;
}  // namespace blink

namespace ui {
namespace test {
class SnapFlingControllerTest;
}

class SnapFlingCurve;

// A client that provides information to the controller. It also executes the
// scroll operations and requests animation frames.
class SnapFlingClient {
 public:
  virtual bool GetSnapFlingInfo(const gfx::Vector2dF& natural_displacement,
                                gfx::Vector2dF* initial_offset,
                                gfx::Vector2dF* target_offset) const = 0;
  virtual gfx::Vector2dF ScrollByForSnapFling(const gfx::Vector2dF& delta) = 0;
  virtual void ScrollEndForSnapFling() = 0;
  virtual void RequestAnimationForSnapFling() = 0;
};

class SnapFlingController {
 public:
  explicit SnapFlingController(SnapFlingClient* client);

  static std::unique_ptr<SnapFlingController> CreateForTests(
      SnapFlingClient* client,
      std::unique_ptr<SnapFlingCurve> curve);

  ~SnapFlingController();

  // Returns true if the event should be consumed for snapping and should not be
  // processed further.
  bool FilterEventForSnap(const blink::WebInputEvent& event);

  // Creates the snap fling curve from the first inertial GSU. Returns true if
  // the event if a snap fling curve has been created and the event should not
  // be processed further.
  bool HandleGestureScrollUpdate(const blink::WebGestureEvent& event);

  // Notifies the snap fling controller to update or end the scroll animation.
  void Animate(base::TimeTicks time);

 private:
  friend class test::SnapFlingControllerTest;

  enum class State {
    // We haven't received an inertial GSU in this scroll sequence.
    kIdle,
    // We have received an inertial GSU but decided not to snap for this scroll
    // sequence.
    kIgnored,
    // We have received an inertial GSU and decided to snap and animate it for
    // this scroll sequence. So subsequent GSUs and GSE in the scroll sequence
    // are consumed for snapping.
    kActive,
    // The animation of the snap fling has finished for this scroll sequence.
    // Subsequent GSUs and GSE in the scroll sequence are ignored.
    kFinished,
  };

  SnapFlingController(SnapFlingClient* client,
                      std::unique_ptr<SnapFlingCurve> curve);
  void ClearSnapFling();

  // Sets the |curve_| to |curve| and the |state| to |kActive|.
  void SetCurveForTest(std::unique_ptr<SnapFlingCurve> curve);

  void SetActiveStateForTest() { state_ = State::kActive; }

  SnapFlingClient* client_;
  State state_ = State::kIdle;
  std::unique_ptr<SnapFlingCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(SnapFlingController);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_SNAP_FLING_CONTROLLER_H_