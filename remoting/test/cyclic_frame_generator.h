// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_
#define REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {
namespace test {

// CyclicFrameGenerator generates a sequence of frames that approximates
// properties of a real video stream when using a desktop applications. It
// loads a sequence of reference frames and switches between them with the
// specified frequency (every 2 seconds by default). Between reference frames it
// also generate frames with small changes which simulate a blinking cursor.
class CyclicFrameGenerator
    : public base::RefCountedThreadSafe<CyclicFrameGenerator> {
 public:
  enum class FrameType {
    // Frame had no changes.
    EMPTY,

    // Whole screen changed.
    FULL,

    // Cursor state has changed.
    CURSOR,
  };

  static scoped_refptr<CyclicFrameGenerator> Create();

  CyclicFrameGenerator(
      std::vector<scoped_ptr<webrtc::DesktopFrame>> reference_frames);

  void set_frame_cycle_period(base::TimeDelta frame_cycle_period) {
    frame_cycle_period_ = frame_cycle_period;
  }

  void set_cursor_blink_period(base::TimeDelta cursor_blink_period) {
    cursor_blink_period_ = cursor_blink_period;
  }

  void SetTickClock(base::TickClock* tick_clock);

  scoped_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::DesktopCapturer::Callback* callback);

  FrameType last_frame_type() { return last_frame_type_; }

 private:
  ~CyclicFrameGenerator();
  friend class base::RefCountedThreadSafe<CyclicFrameGenerator>;

  std::vector<scoped_ptr<webrtc::DesktopFrame>> reference_frames_;
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_;
  webrtc::DesktopSize screen_size_;

  // By default switch between reference frames every 2 seconds.
  base::TimeDelta frame_cycle_period_ = base::TimeDelta::FromSeconds(2);

  // By default blink the cursor 4 times per seconds.
  base::TimeDelta cursor_blink_period_ = base::TimeDelta::FromMilliseconds(250);

  // Index of the reference frame used to render the last generated frame.
  int last_reference_frame_ = -1;

  // True if the cursor was rendered on the last generated frame.
  bool last_cursor_state_ = false;

  FrameType last_frame_type_ = FrameType::EMPTY;

  base::TimeTicks started_time_;

  DISALLOW_COPY_AND_ASSIGN(CyclicFrameGenerator);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_
