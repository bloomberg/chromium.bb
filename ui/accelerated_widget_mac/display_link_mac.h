// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_

#include <QuartzCore/CVDisplayLink.h>

#include <map>

#include "base/mac/scoped_typeref.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {

class ACCELERATED_WIDGET_MAC_EXPORT DisplayLinkMac
    : public base::RefCountedThreadSafe<DisplayLinkMac> {
 public:
  // This must only be called from the main thread.
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Get vsync scheduling parameters.
  bool GetVSyncParameters(
      base::TimeTicks* timebase,
      base::TimeDelta* interval);

  // The vsync parameters are cached, because re-computing them is expensive.
  // The parameters also skew over time (astonishingly quickly -- 0.1 msec per
  // second), so, use this method to tell the display link the current time.
  // If too much time has elapsed since the last time the vsync parameters were
  // calculated, re-calculate them.
  void NotifyCurrentTime(const base::TimeTicks& now);

 private:
  friend class base::RefCountedThreadSafe<DisplayLinkMac>;

  DisplayLinkMac(
      CGDirectDisplayID display_id,
      base::ScopedTypeRef<CVDisplayLinkRef> display_link);
  virtual ~DisplayLinkMac();

  void StartOrContinueDisplayLink();
  void StopDisplayLink();

  // Looks up the display and calls UpdateVSyncParameters() on the corresponding
  // DisplayLinkMac.
  static void DoUpdateVSyncParameters(CGDirectDisplayID display,
                                      const CVTimeStamp& time);

  // Processes the display link callback.
  void UpdateVSyncParameters(const CVTimeStamp& time);

  // Each display link instance consumes a non-negligible number of cycles, so
  // make all display links on the same screen share the same object. This map
  // must only be changed from the main thread.
  //
  // Note that this is a weak map, holding non-owning pointers to the
  // DisplayLinkMac objects. DisplayLinkMac is a ref-counted class, and is
  // jointly owned by the various callers that got a copy by calling
  // GetForDisplay().
  using DisplayLinkMap = std::map<CGDirectDisplayID, DisplayLinkMac*>;
  static DisplayLinkMap& GetAllDisplayLinks();

  // The task runner to post tasks to from the display link thread.
  static scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner();

  // Called by the system on the display link thread, and posts a call to
  // DoUpdateVSyncParameters() to the UI thread.
  static CVReturn DisplayLinkCallback(
      CVDisplayLinkRef display_link,
      const CVTimeStamp* now,
      const CVTimeStamp* output_time,
      CVOptionFlags flags_in,
      CVOptionFlags* flags_out,
      void* context);

  // This is called whenever the display is reconfigured, and marks that the
  // vsync parameters must be recalculated.
  static void DisplayReconfigurationCallBack(
      CGDirectDisplayID display,
      CGDisplayChangeSummaryFlags flags,
      void* user_info);

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // VSync parameters computed during UpdateVSyncParameters().
  bool timebase_and_interval_valid_ = false;
  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  // The time after which we should re-start the display link to get fresh
  // parameters.
  base::TimeTicks recalculate_time_;
};

}  // ui

#endif  // UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_
