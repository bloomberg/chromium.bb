// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/time/time.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

struct MockMotionEvent : public MotionEvent {
  enum { MAX_POINTERS = 3 };

  MockMotionEvent();
  explicit MockMotionEvent(Action action);
  MockMotionEvent(Action action, base::TimeTicks time, float x, float y);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  float x0,
                  float y0,
                  float x1,
                  float y1);
  MockMotionEvent(const MockMotionEvent& other);
  virtual ~MockMotionEvent();

  // MotionEvent methods.
  virtual Action GetAction() const OVERRIDE;
  virtual int GetActionIndex() const OVERRIDE;
  virtual size_t GetPointerCount() const OVERRIDE;
  virtual int GetId() const OVERRIDE;
  virtual int GetPointerId(size_t pointer_index) const OVERRIDE;
  virtual float GetX(size_t pointer_index) const OVERRIDE;
  virtual float GetY(size_t pointer_index) const OVERRIDE;
  virtual float GetTouchMajor(size_t pointer_index) const OVERRIDE;
  virtual float GetPressure(size_t pointer_index) const OVERRIDE;
  virtual base::TimeTicks GetEventTime() const OVERRIDE;
  virtual size_t GetHistorySize() const OVERRIDE;
  virtual base::TimeTicks GetHistoricalEventTime(size_t historical_index) const
      OVERRIDE;
  virtual float GetHistoricalTouchMajor(size_t pointer_index,
                                        size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalX(size_t pointer_index,
                               size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalY(size_t pointer_index,
                               size_t historical_index) const OVERRIDE;

  virtual scoped_ptr<MotionEvent> Clone() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Cancel() const OVERRIDE;

  // Utility methods.
  void SetId(int new_id);
  void PressPoint(float x, float y);
  void MovePoint(size_t index, float x, float y);
  void ReleasePoint();
  void CancelPoint();

  MotionEvent::Action action;
  size_t pointer_count;
  gfx::PointF points[MAX_POINTERS];
  base::TimeTicks time;
  int id;
};

}  // namespace ui
