// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
#define REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_

#include <stddef.h>

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
  DesktopDisplayInfo(DesktopDisplayInfo&&);
  DesktopDisplayInfo& operator=(DesktopDisplayInfo&&);
  ~DesktopDisplayInfo();

  static webrtc::DesktopSize CalcSizeDips(webrtc::DesktopSize size,
                                          int dpi_x,
                                          int dpi_y);

  // Clear out the display info.
  void Reset();
  int NumDisplays() const;
  const DisplayGeometry* GetDisplayInfo(unsigned int id) const;

  // Calculate the offset to the origin (upper left) of the specific display.
  webrtc::DesktopVector CalcDisplayOffset(webrtc::ScreenId id) const;

  // Add a new display with the given info to the display list.
  void AddDisplay(const DisplayGeometry& display);

  void AddDisplayFrom(const protocol::VideoTrackLayout& track);

  bool operator==(const DesktopDisplayInfo& other) const;
  bool operator!=(const DesktopDisplayInfo& other) const;

  const std::vector<DisplayGeometry>& displays() const { return displays_; }

 private:
  std::vector<DisplayGeometry> displays_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_DISPLAY_INFO_H_
