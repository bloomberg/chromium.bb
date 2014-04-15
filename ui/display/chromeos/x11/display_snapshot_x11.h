// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_DISPLAY_SNAPSHOT_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_DISPLAY_SNAPSHOT_X11_H_

#include "ui/display/display_export.h"
#include "ui/display/types/chromeos/display_snapshot.h"

// Forward declare from Xlib and Xrandr.
typedef unsigned long XID;
typedef XID RROutput;
typedef XID RRCrtc;

namespace ui {

class DISPLAY_EXPORT DisplaySnapshotX11 : public DisplaySnapshot {
 public:
  DisplaySnapshotX11(int64_t display_id,
                     bool has_proper_display_id,
                     const gfx::Point& origin,
                     const gfx::Size& physical_size,
                     DisplayConnectionType type,
                     bool is_aspect_preserving_scaling,
                     bool has_overscan,
                     std::string display_name,
                     const std::vector<const DisplayMode*>& modes,
                     const DisplayMode* current_mode,
                     const DisplayMode* native_mode,
                     RROutput output,
                     RRCrtc crtc,
                     int index);
  virtual ~DisplaySnapshotX11();

  RROutput output() const { return output_; }
  RRCrtc crtc() const { return crtc_; }
  int index() const { return index_; }

  // DisplaySnapshot overrides:
  virtual std::string ToString() const OVERRIDE;

 private:
  RROutput output_;

  // CRTC that should be used for this output. Not necessarily the CRTC
  // that XRandR reports is currently being used.
  RRCrtc crtc_;

  // This output's index in the array returned by XRandR. Stable even as
  // outputs are connected or disconnected.
  int index_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshotX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_DISPLAY_SNAPSHOT_X11_H_
