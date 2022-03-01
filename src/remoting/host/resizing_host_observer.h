// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
#define REMOTING_HOST_RESIZING_HOST_OBSERVER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/base/screen_controls.h"
#include "remoting/host/base/screen_resolution.h"

namespace base {
class TickClock;
}

namespace remoting {

class DesktopResizer;

// TODO(alexeypa): Rename this class to reflect that it is not
// HostStatusObserver any more.

// Uses the specified DesktopResizer to match host desktop size to the client
// view size as closely as is possible. When the connection closes, restores
// the original desktop size if restore is true.
class ResizingHostObserver : public ScreenControls {
 public:
  explicit ResizingHostObserver(
      std::unique_ptr<DesktopResizer> desktop_resizer,
      bool restore);

  ResizingHostObserver(const ResizingHostObserver&) = delete;
  ResizingHostObserver& operator=(const ResizingHostObserver&) = delete;

  ~ResizingHostObserver() override;

  // ScreenControls interface.
  void SetScreenResolution(const ScreenResolution& resolution) override;

  // Provide a replacement for base::TimeTicks::Now so that this class can be
  // unit-tested in a timely manner. This function will be called exactly
  // once for each call to SetScreenResolution.
  void SetClockForTesting(const base::TickClock* clock);

 private:
  void RestoreScreenResolution();

  std::unique_ptr<DesktopResizer> desktop_resizer_;
  ScreenResolution original_resolution_;
  bool restore_;

  // State to manage rate-limiting of desktop resizes.
  base::OneShotTimer deferred_resize_timer_;
  base::TimeTicks previous_resize_time_;
  raw_ptr<const base::TickClock> clock_;

  base::WeakPtrFactory<ResizingHostObserver> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
