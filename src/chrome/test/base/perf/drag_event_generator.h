// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_
#define CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_

#include "base/run_loop.h"
#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point.h"

namespace ui_test_utils {

// A utility class that generates drag events using |producer| logic
// at a rate of 60 events per seconds.
class DragEventGenerator {
 public:
  // Producer produces the point for given progress value. The range of
  // the progress is between 0.f (start) to 1.f (end).
  class PointProducer {
   public:
    virtual ~PointProducer();

    // Returns the position at |progression|.
    virtual gfx::Point GetPosition(float progress) = 0;

    // Returns the duration this produce should be used.
    virtual const base::TimeDelta GetDuration() const = 0;
  };

  DragEventGenerator(std::unique_ptr<PointProducer> producer,
                     bool touch = false);
  ~DragEventGenerator();

  void Wait();

 private:
  void Done(const gfx::Point position);
  void GenerateNext();

  std::unique_ptr<PointProducer> producer_;
  int count_ = 0;

  const base::TimeTicks start_;
  base::TimeTicks expected_next_time_;
  const bool touch_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DragEventGenerator);
};

// InterpolatedProducer produces the interpolated location between two points
// based on tween type.
class InterpolatedProducer : public DragEventGenerator::PointProducer {
 public:
  InterpolatedProducer(const gfx::Point& start,
                       const gfx::Point& end,
                       const base::TimeDelta duration,
                       gfx::Tween::Type type = gfx::Tween::LINEAR);
  ~InterpolatedProducer() override;

  // PointProducer:
  gfx::Point GetPosition(float progress) override;
  const base::TimeDelta GetDuration() const override;

 private:
  gfx::Point start_, end_;
  base::TimeDelta duration_;
  gfx::Tween::Type type_;

  DISALLOW_COPY_AND_ASSIGN(InterpolatedProducer);
};

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_PERF_DRAG_EVENT_GENERATOR_H_
