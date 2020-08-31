// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_
#define UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_

namespace aura {

class WindowTreeHost;

// This class is a shim between WindowOcclusionTracker and os-specific
// window occlusion tracking classes (currently just
// NativeWindowOcclusionTrackerWin).
class NativeWindowOcclusionTracker {
 public:
  NativeWindowOcclusionTracker();
  virtual ~NativeWindowOcclusionTracker();

  // Enables native window occlusion tracking for the native window |host|
  // represents.
  void EnableNativeWindowOcclusionTracking(WindowTreeHost* host);

  // Disables native window occlusion tracking for the native window |host|
  // represents.
  void DisableNativeWindowOcclusionTracking(WindowTreeHost* host);
};

}  // namespace aura

#endif  // UI_AURA_NATIVE_WINDOW_OCCLUSION_TRACKER_H_
