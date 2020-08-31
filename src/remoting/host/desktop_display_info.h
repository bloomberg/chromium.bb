// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_

#include <stddef.h>

#include "base/logging.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

struct DisplayGeometry {
  int32_t id;
  int32_t x, y;
  uint32_t width, height;
  uint32_t dpi;     // Number of pixels per logical inch.
  uint32_t bpp;     // Number of bits per pixel.
  bool is_default;  // True if this is the default display.
};

class DesktopDisplayInfo {
 public:
  DesktopDisplayInfo();
  ~DesktopDisplayInfo();

  static webrtc::DesktopSize CalcSizeDips(webrtc::DesktopSize size,
                                          int dpi_x,
                                          int dpi_y);

  // Clear out the display info.
  void Reset();
  int NumDisplays();
  const DisplayGeometry* GetDisplayInfo(unsigned int id);

  // Calculate the offset to the origin (upper left) of the specific display.
  webrtc::DesktopVector CalcDisplayOffset(webrtc::ScreenId id);

  // Add a new display with the given info to the display list.
  void AddDisplay(std::unique_ptr<DisplayGeometry> display);

  void AddDisplayFrom(protocol::VideoTrackLayout track);

  // Query the OS for the set of currently active desktop displays.
  void LoadCurrentDisplayInfo();

  bool operator==(const DesktopDisplayInfo& other);
  bool operator!=(const DesktopDisplayInfo& other);

  const std::vector<std::unique_ptr<DisplayGeometry>>& displays() const {
    return displays_;
  }

 private:
  std::vector<std::unique_ptr<DisplayGeometry>> displays_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDisplayInfo);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
