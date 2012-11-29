// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_
#define UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ui/aura/root_window_host.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/win/window_impl.h"

namespace ui {
class ViewProp;
}

namespace aura {
// RootWindowHost implementaton that receives events from a different process.
class AURA_EXPORT RemoteRootWindowHostWin : public RootWindowHost {
 public:
  static RemoteRootWindowHostWin* Instance();
  static RemoteRootWindowHostWin* Create(const gfx::Rect& bounds);

  void OnMouseMoved(int32 x, int32 y, int32 flags);
  void OnMouseButton(
      int32 x, int32 y, int32 extra, ui::EventType type, ui::EventFlags flags);
  void OnKeyDown(uint32 vkey,
                 uint32 repeat_count,
                 uint32 scan_code,
                 uint32 flags);
  void OnKeyUp(uint32 vkey,
               uint32 repeat_count,
               uint32 scan_code,
               uint32 flags);
  void OnChar(uint32 key_code,
              uint32 repeat_count,
              uint32 scan_code,
              uint32 flags);
  void OnVisibilityChanged(bool visible);

 private:
  RemoteRootWindowHostWin(const gfx::Rect& bounds);
  virtual ~RemoteRootWindowHostWin();

  // RootWindowHost overrides:
  virtual void SetDelegate(RootWindowHostDelegate* delegate) OVERRIDE;
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void SetFocusWhenShown(bool focus_when_shown) OVERRIDE;
  virtual bool CopyAreaToSkCanvas(const gfx::Rect& source_bounds,
                                  const gfx::Point& dest_offset,
                                  SkCanvas* canvas) OVERRIDE;
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  RootWindowHostDelegate* delegate_;
  scoped_ptr<ui::ViewProp> prop_;

  DISALLOW_COPY_AND_ASSIGN(RemoteRootWindowHostWin);
};

}  // namespace aura

#endif  // UI_AURA_REMOTE_ROOT_WINDOW_HOST_WIN_H_
