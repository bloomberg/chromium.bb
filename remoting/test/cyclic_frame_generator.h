// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_
#define REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
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
  enum class ChangeType {
    // No changes.
    NO_CHANGES,

    // Whole screen changed.
    FULL,

    // Cursor state has changed.
    CURSOR,
  };

  struct ChangeInfo {
    ChangeInfo();
    ChangeInfo(ChangeType type, base::TimeTicks timestamp);

    ChangeType type = ChangeType::NO_CHANGES;
    base::TimeTicks timestamp;
  };

  typedef std::vector<ChangeInfo> ChangeInfoList;

  static scoped_refptr<CyclicFrameGenerator> Create();

  CyclicFrameGenerator(
      std::vector<std::unique_ptr<webrtc::DesktopFrame>> reference_frames);

  void set_frame_cycle_period(base::TimeDelta frame_cycle_period) {
    frame_cycle_period_ = frame_cycle_period;
  }

  void set_cursor_blink_period(base::TimeDelta cursor_blink_period) {
    cursor_blink_period_ = cursor_blink_period;
  }

  void SetTickClock(base::TickClock* tick_clock);

  // When |draw_barcode| is set to true a barcode is drawn on each generated
  // frame. This makes it possible to call GetChangeList() to identify the frame
  // by its content.
  void set_draw_barcode(bool draw_barcode) { draw_barcode_ = draw_barcode; }

  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::SharedMemoryFactory* shared_memory_factory);

  ChangeType last_frame_type() { return last_frame_type_; }

  // Identifies |frame| by its content and returns list of ChangeInfo for all
  // changes between the frame passed to the previous GetChangeList() call and
  // this one. GetChangeList() must be called for the frames in the order in
  // which they were received, which is expected to match the order in which
  // they are generated.
  ChangeInfoList GetChangeList(webrtc::DesktopFrame* frame);

 private:
  ~CyclicFrameGenerator();
  friend class base::RefCountedThreadSafe<CyclicFrameGenerator>;

  std::vector<std::unique_ptr<webrtc::DesktopFrame>> reference_frames_;
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_;
  webrtc::DesktopSize screen_size_;

  // By default switch between reference frames every 2 seconds.
  base::TimeDelta frame_cycle_period_ = base::TimeDelta::FromSeconds(2);

  // By default blink the cursor 4 times per seconds.
  base::TimeDelta cursor_blink_period_ = base::TimeDelta::FromMilliseconds(250);

  // Id of the last frame encoded on the barcode.
  int last_frame_id_ = -1;

  // Index of the reference frame used to render the last generated frame.
  int last_reference_frame_ = -1;

  // True if the cursor was rendered on the last generated frame.
  bool last_cursor_state_ = false;

  ChangeType last_frame_type_ = ChangeType::NO_CHANGES;

  bool draw_barcode_ = false;

  base::TimeTicks started_time_;

  // frame_id of the frame passed to the last GetChangeList() call.
  int last_identifier_frame_ = -1;

  DISALLOW_COPY_AND_ASSIGN(CyclicFrameGenerator);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_CYCLIC_FRAME_GENERATOR_H_
