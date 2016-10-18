// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_
#define REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_

#include <memory>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {
namespace test {

class ScrollFrameGenerator
    : public base::RefCountedThreadSafe<ScrollFrameGenerator> {
 public:
  ScrollFrameGenerator();

  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::SharedMemoryFactory* shared_memory_factory);

  base::TimeDelta GetFrameLatency(const webrtc::DesktopFrame& frame);

 private:
  ~ScrollFrameGenerator();
  friend class base::RefCountedThreadSafe<ScrollFrameGenerator>;

  std::unique_ptr<webrtc::DesktopFrame> base_frame_;
  base::TimeTicks start_time_;

  std::unordered_map<int, base::TimeTicks> frame_timestamp_;

  // Id of the last frame encoded on the barcode.
  int last_frame_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(ScrollFrameGenerator);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_SCROLL_FRAME_GENERATOR_H_
