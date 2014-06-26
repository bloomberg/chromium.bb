// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
namespace test {

struct MockMotionEvent : public MotionEvent {
  enum { MAX_POINTERS = 3 };
  enum { TOUCH_MAJOR = 10 };

  MockMotionEvent();
  explicit MockMotionEvent(Action action);
  MockMotionEvent(Action action, base::TimeTicks time, float x, float y);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  float x0,
                  float y0,
                  float x1,
                  float y1);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  float x0,
                  float y0,
                  float x1,
                  float y1,
                  float x2,
                  float y2);
  MockMotionEvent(Action action,
                  base::TimeTicks time,
                  const std::vector<gfx::PointF>& positions);
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
  virtual float GetRawX(size_t pointer_index) const OVERRIDE;
  virtual float GetRawY(size_t pointer_index) const OVERRIDE;
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
  virtual ToolType GetToolType(size_t pointer_index) const OVERRIDE;
  virtual int GetButtonState() const OVERRIDE;

  virtual scoped_ptr<MotionEvent> Clone() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Cancel() const OVERRIDE;

  // Utility methods.
  void SetId(int new_id);
  void SetTime(base::TimeTicks new_time);
  void PressPoint(float x, float y);
  void MovePoint(size_t index, float x, float y);
  void ReleasePoint();
  void CancelPoint();
  void SetTouchMajor(float new_touch_major);
  void SetRawOffset(float raw_offset_x, float raw_offset_y);
  void SetToolType(size_t index, ToolType tool_type);
  void SetButtonState(int button_state);

  MotionEvent::Action action;
  size_t pointer_count;
  gfx::PointF points[MAX_POINTERS];
  ToolType tool_types[MAX_POINTERS];
  gfx::Vector2dF raw_offset;
  base::TimeTicks time;
  float touch_major;
  int id;
  int button_state;
};

}  // namespace test
}  // namespace ui
