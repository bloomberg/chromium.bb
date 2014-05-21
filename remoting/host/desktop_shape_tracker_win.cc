// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_shape_tracker.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

struct EnumDesktopShapeData {
  EnumDesktopShapeData()
    : window_region(CreateRectRgn(0, 0, 0, 0)),
      desktop_region(CreateRectRgn(0, 0, 0, 0)) {
  }
  base::win::ScopedRegion window_region;
  base::win::ScopedRegion desktop_region;
};

class DesktopShapeTrackerWin : public DesktopShapeTracker {
 public:
  DesktopShapeTrackerWin();
  virtual ~DesktopShapeTrackerWin();

  virtual void RefreshDesktopShape();
  virtual const webrtc::DesktopRegion& desktop_shape();

 private:
  // Callback passed to EnumWindows() to enumerate windows.
  static BOOL CALLBACK EnumWindowsCallback(HWND window, LPARAM lparam);

  // The most recently calculated desktop region.
  webrtc::DesktopRegion desktop_shape_;

  // Stored to compare with newly calculated desktop shapes, to avoid converting
  // to an DesktopRegion unless the shape has actually changed.
  base::win::ScopedRegion old_desktop_region_;

  DISALLOW_COPY_AND_ASSIGN(DesktopShapeTrackerWin);
};

DesktopShapeTrackerWin::DesktopShapeTrackerWin()
  : old_desktop_region_(CreateRectRgn(0, 0, 0, 0)) {
}

DesktopShapeTrackerWin::~DesktopShapeTrackerWin() {
}

void DesktopShapeTrackerWin::RefreshDesktopShape() {
  // Accumulate a new desktop shape from current window positions.
  scoped_ptr<EnumDesktopShapeData> shape_data(new EnumDesktopShapeData);
  if (!EnumWindows(EnumWindowsCallback, (LPARAM)shape_data.get())) {
    PLOG(ERROR) << "Failed to enumerate windows";
    desktop_shape_.Clear();
    return;
  }

  // If the shape has changed, refresh |desktop_shape_|.
  if (!EqualRgn(shape_data->desktop_region, old_desktop_region_)) {
    old_desktop_region_.Set(shape_data->desktop_region.release());

    // Determine the size of output buffer required to receive the region.
    DWORD bytes_size = GetRegionData(old_desktop_region_, 0, NULL);
    CHECK(bytes_size != 0);

    // Fetch the Windows RECTs that comprise the region.
    std::vector<char> buffer(bytes_size);
    LPRGNDATA region_data = reinterpret_cast<LPRGNDATA>(buffer.data());
    DWORD result = GetRegionData(old_desktop_region_, bytes_size, region_data);
    CHECK(result == bytes_size);
    const LPRECT rects = reinterpret_cast<LPRECT>(&region_data->Buffer[0]);

    // Reset |desktop_shape_| and add new rectangles into it.
    desktop_shape_.Clear();
    for (size_t i = 0; i < region_data->rdh.nCount; ++i) {
      desktop_shape_.AddRect(webrtc::DesktopRect::MakeLTRB(
          rects[i].left, rects[i].top, rects[i].right, rects[i].bottom));
    }
  }
}

const webrtc::DesktopRegion& DesktopShapeTrackerWin::desktop_shape() {
  return desktop_shape_;
}

// static
BOOL DesktopShapeTrackerWin::EnumWindowsCallback(HWND window, LPARAM lparam) {
  EnumDesktopShapeData* data = reinterpret_cast<EnumDesktopShapeData*>(lparam);
  HRGN desktop_region = data->desktop_region.Get();
  HRGN window_region = data->window_region.Get();

  // Is the window visible?
  if (!IsWindow(window) || !IsWindowVisible(window) || IsIconic(window))
    return TRUE;

  // Find the desktop position of the window (including non-client-area).
  RECT window_rect;
  if (!GetWindowRect(window, &window_rect))
    return TRUE;

  // Find the shape of the window, in window coords.
  // GetWindowRgn will overwrite the current contents of |window_region|.
  if (GetWindowRgn(window, window_region) != ERROR) {
    // Translate the window region into desktop coordinates.
    OffsetRgn(window_region, window_rect.left, window_rect.top);
  } else {
    // Window has no shape, or an error occurred, so assume it's rectangular.
    SetRectRgn(window_region, window_rect.left, window_rect.top,
               window_rect.right, window_rect.bottom);
  }

  // TODO(wez): If the window is maximized then we should clip it to the
  // display on which it is maximized.
  //  if (IsZoomed(window))
  //    CombineRgn(window_region, window_region, screen_region, RGN_AND);

  // Merge the window region into the accumulated desktop region.  Window
  // regions are combined together before converting the result to
  // DesktopRegion. It assumed that this approach is more efficient than
  // converting each window region individually.
  CombineRgn(desktop_region, desktop_region, window_region, RGN_OR);

  return TRUE;
}

} // namespace

// static
scoped_ptr<DesktopShapeTracker> DesktopShapeTracker::Create(
    webrtc::DesktopCaptureOptions options) {
  return scoped_ptr<DesktopShapeTracker>(new DesktopShapeTrackerWin());
}

}  // namespace remoting
