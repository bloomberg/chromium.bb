// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <windows.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/native_library.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_gdi_object.h"
#include "remoting/base/capture_data.h"
#include "remoting/host/capturer_helper.h"
#include "remoting/host/desktop_win.h"
#include "remoting/host/differ.h"
#include "remoting/host/scoped_thread_desktop_win.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

namespace {

// Constants from dwmapi.h.
const UINT DWM_EC_DISABLECOMPOSITION = 0;
const UINT DWM_EC_ENABLECOMPOSITION = 1;

typedef HRESULT (WINAPI * DwmEnableCompositionFunc)(UINT);

const wchar_t kDwmapiLibraryName[] = L"dwmapi";

// Pixel colors used when generating cursor outlines.
const uint32 kPixelBgraBlack = 0xff000000;
const uint32 kPixelBgraWhite = 0xffffffff;
const uint32 kPixelBgraTransparent = 0x00000000;

// CapturerGdi captures 32bit RGB using GDI.
//
// CapturerGdi is double-buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerGdi : public Capturer {
 public:
  CapturerGdi();
  virtual ~CapturerGdi();

  // Capturer interface.
  virtual void Start(const CursorShapeChangedCallback& callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void ScreenConfigurationChanged() OVERRIDE;
  virtual media::VideoFrame::Format pixel_format() const OVERRIDE;
  virtual void ClearInvalidRegion() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void InvalidateScreen(const SkISize& size) OVERRIDE;
  virtual void InvalidateFullScreen() OVERRIDE;
  virtual void CaptureInvalidRegion(
      const CaptureCompletedCallback& callback) OVERRIDE;
  virtual const SkISize& size_most_recent() const OVERRIDE;

 private:
  struct VideoFrameBuffer {
    VideoFrameBuffer(void* data, const SkISize& size, int bytes_per_pixel,
                     int bytes_per_row)
      : data(data), size(size), bytes_per_pixel(bytes_per_pixel),
        bytes_per_row(bytes_per_row) {
    }
    VideoFrameBuffer() {
      data = 0;
      size = SkISize::Make(0, 0);
      bytes_per_pixel = 0;
      bytes_per_row = 0;
    }
    void* data;
    SkISize size;
    int bytes_per_pixel;
    int bytes_per_row;
  };

  // Make sure that the current buffer has the same size as the screen.
  void UpdateBufferCapture(const SkISize& size);

  // Allocate memory for a buffer of a given size, freeing any memory previously
  // allocated for that buffer.
  void ReallocateBuffer(int buffer_index, const SkISize& size);

  void CalculateInvalidRegion();
  void CaptureRegion(const SkRegion& region,
                     const CaptureCompletedCallback& callback);

  void ReleaseBuffers();
  // Generates an image in the current buffer.
  void CaptureImage();

  // Expand the cursor shape to add a white outline for visibility against
  // dark backgrounds.
  void AddCursorOutline(int width, int height, uint32* dst);

  // Capture the current cursor shape.
  void CaptureCursor();

  // Gets the screen size.
  SkISize GetScreenSize();

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper_;

  // Callback notified whenever the cursor shape is changed.
  CursorShapeChangedCallback cursor_shape_changed_callback_;

  // Snapshot of the last cursor bitmap we sent to the client. This is used
  // to diff against the current cursor so we only send a cursor-change
  // message when the shape has changed.
  scoped_array<uint8> last_cursor_;
  SkISize last_cursor_size_;

  // There are two buffers for the screen images, as required by Capturer.
  static const int kNumBuffers = 2;
  VideoFrameBuffer buffers_[kNumBuffers];

  ScopedThreadDesktopWin desktop_;

  // Gdi specific information about screen.
  HWND desktop_window_;
  HDC desktop_dc_;
  HDC memory_dc_;
  HBITMAP target_bitmap_[kNumBuffers];

  // The screen size attached to the device contexts through which the screen
  // is captured.
  SkISize dc_size_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  base::NativeLibrary dwmapi_library_;
  DwmEnableCompositionFunc composition_func_;

  DISALLOW_COPY_AND_ASSIGN(CapturerGdi);
};

// 3780 pixels per meter is equivalent to 96 DPI, typical on desktop monitors.
static const int kPixelsPerMeter = 3780;
// 32 bit RGBA is 4 bytes per pixel.
static const int kBytesPerPixel = 4;

CapturerGdi::CapturerGdi()
    : last_cursor_size_(SkISize::Make(0, 0)),
      desktop_window_(NULL),
      desktop_dc_(NULL),
      memory_dc_(NULL),
      dc_size_(SkISize::Make(0, 0)),
      current_buffer_(0),
      pixel_format_(media::VideoFrame::RGB32),
      dwmapi_library_(NULL),
      composition_func_(NULL) {
  memset(target_bitmap_, 0, sizeof(target_bitmap_));
  memset(buffers_, 0, sizeof(buffers_));
  ScreenConfigurationChanged();
}

CapturerGdi::~CapturerGdi() {
  ReleaseBuffers();

  if (dwmapi_library_ != NULL) {
    base::UnloadNativeLibrary(dwmapi_library_);
  }
}

media::VideoFrame::Format CapturerGdi::pixel_format() const {
  return pixel_format_;
}

void CapturerGdi::ClearInvalidRegion() {
  helper_.ClearInvalidRegion();
}

void CapturerGdi::InvalidateRegion(const SkRegion& invalid_region) {
  helper_.InvalidateRegion(invalid_region);
}

void CapturerGdi::InvalidateScreen(const SkISize& size) {
  helper_.InvalidateScreen(size);
}

void CapturerGdi::InvalidateFullScreen() {
  helper_.InvalidateFullScreen();
}

void CapturerGdi::CaptureInvalidRegion(
    const CaptureCompletedCallback& callback) {
  // Force the system to power-up display hardware, if it has been suspended.
  SetThreadExecutionState(ES_DISPLAY_REQUIRED);

  // Perform the capture.
  CalculateInvalidRegion();
  SkRegion invalid_region;
  helper_.SwapInvalidRegion(&invalid_region);
  CaptureRegion(invalid_region, callback);

  // Check for cursor shape update.
  CaptureCursor();
}

const SkISize& CapturerGdi::size_most_recent() const {
  return helper_.size_most_recent();
}

void CapturerGdi::ReleaseBuffers() {
  for (int i = kNumBuffers - 1; i >= 0; i--) {
    if (target_bitmap_[i]) {
      DeleteObject(target_bitmap_[i]);
      target_bitmap_[i] = NULL;
    }
    if (buffers_[i].data) {
      DeleteObject(buffers_[i].data);
      buffers_[i].data = NULL;
    }
  }

  if (desktop_dc_) {
    ReleaseDC(desktop_window_, desktop_dc_);
    desktop_window_ = NULL;
    desktop_dc_ = NULL;
  }

  if (memory_dc_) {
    DeleteDC(memory_dc_);
    memory_dc_ = NULL;
  }
}

void CapturerGdi::Start(
    const CursorShapeChangedCallback& callback) {
  cursor_shape_changed_callback_ = callback;

  // Load dwmapi.dll dynamically since it is not available on XP.
  if (dwmapi_library_ == NULL) {
    std::string error;
    dwmapi_library_ = base::LoadNativeLibrary(
        FilePath(base::GetNativeLibraryName(WideToUTF16(kDwmapiLibraryName))),
        &error);
  }

  if (dwmapi_library_ != NULL && composition_func_ == NULL) {
    composition_func_ = reinterpret_cast<DwmEnableCompositionFunc>(
        base::GetFunctionPointerFromNativeLibrary(dwmapi_library_,
                                                  "DwmEnableComposition"));
  }

  // Vote to disable Aero composited desktop effects while capturing. Windows
  // will restore Aero automatically if the process exits. This has no effect
  // under Windows 8 or higher.  See crbug.com/124018.
  if (composition_func_ != NULL) {
    (*composition_func_)(DWM_EC_DISABLECOMPOSITION);
  }
}

void CapturerGdi::Stop() {
  // Restore Aero.
  if (composition_func_ != NULL) {
    (*composition_func_)(DWM_EC_ENABLECOMPOSITION);
  }
}

void CapturerGdi::ScreenConfigurationChanged() {
  // We poll for screen configuration changes, so ignore notifications.
}

void CapturerGdi::UpdateBufferCapture(const SkISize& size) {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<DesktopWin> input_desktop = DesktopWin::GetInputDesktop();
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // Release GDI resources otherwise SetThreadDesktop will fail.
    if (desktop_dc_) {
      ReleaseDC(desktop_window_, desktop_dc_);
      desktop_window_ = NULL;
      desktop_dc_ = NULL;
    }

    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }

    ReleaseBuffers();

    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from a diffented desktop.
    desktop_.SetThreadDesktop(input_desktop.Pass());
  }

  // Make sure the DCs have the correct dimensions.
  if (size != dc_size_) {
    // TODO(simonmorris): screen dimensions changing isn't equivalent to needing
    // a new DC, but it's good enough for now.
    if (desktop_dc_) {
      ReleaseDC(desktop_window_, desktop_dc_);
      desktop_window_ = NULL;
      desktop_dc_ = NULL;
    }

    if (memory_dc_) {
      DeleteDC(memory_dc_);
      memory_dc_ = NULL;
    }
  }

  if (desktop_dc_ == NULL) {
    DCHECK(desktop_window_ == NULL);
    DCHECK(memory_dc_ == NULL);

    desktop_window_ = GetDesktopWindow();
    desktop_dc_ = GetDC(desktop_window_);
    memory_dc_ = CreateCompatibleDC(desktop_dc_);
    dc_size_ = size;
  }

  // Make sure the current bitmap has the correct dimensions.
  if (buffers_[current_buffer_].data == NULL ||
      size != buffers_[current_buffer_].size) {
    ReallocateBuffer(current_buffer_, size);
    InvalidateFullScreen();
  }
}

void CapturerGdi::ReallocateBuffer(int buffer_index, const SkISize& size) {
  // Delete any previously constructed bitmap.
  if (target_bitmap_[buffer_index]) {
    DeleteObject(target_bitmap_[buffer_index]);
    target_bitmap_[buffer_index] = NULL;
  }
  if (buffers_[buffer_index].data) {
    DeleteObject(buffers_[buffer_index].data);
    buffers_[buffer_index].data = NULL;
  }

  // Create a bitmap to keep the desktop image.
  int rounded_width = (size.width() + 3) & (~3);

  // Dimensions of screen.
  pixel_format_ = media::VideoFrame::RGB32;
  int bytes_per_row = rounded_width * kBytesPerPixel;

  // Create a device independent bitmap (DIB) that is the same size.
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biHeight = -size.height();
  bmi.bmiHeader.biWidth = rounded_width;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = kBytesPerPixel * 8;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biSizeImage = bytes_per_row * size.height();
  bmi.bmiHeader.biXPelsPerMeter = kPixelsPerMeter;
  bmi.bmiHeader.biYPelsPerMeter = kPixelsPerMeter;

  // Create memory for the buffers.
  target_bitmap_[buffer_index] =
      CreateDIBSection(desktop_dc_, &bmi, DIB_RGB_COLORS,
                       static_cast<void**>(&buffers_[buffer_index].data),
                       NULL, 0);
  buffers_[buffer_index].size = SkISize::Make(bmi.bmiHeader.biWidth,
                                              std::abs(bmi.bmiHeader.biHeight));
  buffers_[buffer_index].bytes_per_pixel = bmi.bmiHeader.biBitCount / 8;
  buffers_[buffer_index].bytes_per_row =
      bmi.bmiHeader.biSizeImage / std::abs(bmi.bmiHeader.biHeight);
}

void CapturerGdi::CalculateInvalidRegion() {
  CaptureImage();

  const VideoFrameBuffer& current = buffers_[current_buffer_];

  // Find the previous and current screens.
  int prev_buffer_id = current_buffer_ - 1;
  if (prev_buffer_id < 0) {
    prev_buffer_id = kNumBuffers - 1;
  }
  const VideoFrameBuffer& prev = buffers_[prev_buffer_id];

  // Maybe the previous and current screens can't be differenced.
  if ((current.size != prev.size) ||
      (current.bytes_per_pixel != prev.bytes_per_pixel) ||
      (current.bytes_per_row != prev.bytes_per_row)) {
    InvalidateScreen(current.size);
    return;
  }

  // Make sure the differencer is set up correctly for these previous and
  // current screens.
  if (!differ_.get() ||
      (differ_->width() != current.size.width()) ||
      (differ_->height() != current.size.height()) ||
      (differ_->bytes_per_pixel() != current.bytes_per_pixel) ||
      (differ_->bytes_per_row() != current.bytes_per_row)) {
    differ_.reset(new Differ(current.size.width(), current.size.height(),
      current.bytes_per_pixel, current.bytes_per_row));
  }

  SkRegion region;
  differ_->CalcDirtyRegion(prev.data, current.data, &region);

  InvalidateRegion(region);
}

void CapturerGdi::CaptureRegion(const SkRegion& region,
                                const CaptureCompletedCallback& callback) {
  const VideoFrameBuffer& buffer = buffers_[current_buffer_];
  current_buffer_ = (current_buffer_ + 1) % kNumBuffers;

  DataPlanes planes;
  planes.data[0] = static_cast<uint8*>(buffer.data);
  planes.strides[0] = buffer.bytes_per_row;

  scoped_refptr<CaptureData> data(new CaptureData(planes,
                                                  buffer.size,
                                                  pixel_format_));
  data->mutable_dirty_region() = region;

  helper_.set_size_most_recent(data->size());

  callback.Run(data);
}

void CapturerGdi::CaptureImage() {
  // Make sure the structures we use to capture the image have the correct size.
  UpdateBufferCapture(GetScreenSize());

  // Select the target bitmap into the memory dc.
  SelectObject(memory_dc_, target_bitmap_[current_buffer_]);

  // And then copy the rect from desktop to memory.
  BitBlt(memory_dc_, 0, 0, buffers_[current_buffer_].size.width(),
      buffers_[current_buffer_].size.height(), desktop_dc_, 0, 0,
      SRCCOPY | CAPTUREBLT);
}

void CapturerGdi::AddCursorOutline(int width, int height, uint32* dst) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // If this is a transparent pixel (bgr == 0 and alpha = 0), check the
      // neighbor pixels to see if this should be changed to an outline pixel.
      if (*dst == kPixelBgraTransparent) {
        // Change to white pixel if any neighbors (top, bottom, left, right)
        // are black.
        if ((y > 0 && dst[-width] == kPixelBgraBlack) ||
            (y < height - 1 && dst[width] == kPixelBgraBlack) ||
            (x > 0 && dst[-1] == kPixelBgraBlack) ||
            (x < width - 1 && dst[1] == kPixelBgraBlack)) {
          *dst = kPixelBgraWhite;
        }
      }
      dst++;
    }
  }
}

void CapturerGdi::CaptureCursor() {
  CURSORINFO cursor_info;
  cursor_info.cbSize = sizeof(CURSORINFO);
  if (!GetCursorInfo(&cursor_info)) {
    VLOG(3) << "Unable to get cursor info. Error = " << GetLastError();
    return;
  }

  // Note that this does not need to be freed.
  HCURSOR hcursor = cursor_info.hCursor;
  ICONINFO iinfo;
  if (!GetIconInfo(hcursor, &iinfo)) {
    VLOG(3) << "Unable to get cursor icon info. Error = " << GetLastError();
    return;
  }
  int hotspot_x = iinfo.xHotspot;
  int hotspot_y = iinfo.yHotspot;

  // Get the cursor bitmap.
  base::win::ScopedBitmap hbitmap;
  BITMAP bitmap;
  bool color_bitmap;
  if (iinfo.hbmColor) {
    // Color cursor bitmap.
    color_bitmap = true;
    hbitmap.Set((HBITMAP)CopyImage(iinfo.hbmColor, IMAGE_BITMAP, 0, 0,
                                   LR_CREATEDIBSECTION));
    if (!hbitmap.Get()) {
      VLOG(3) << "Unable to copy color cursor image. Error = "
              << GetLastError();
      return;
    }

    // Free the color and mask bitmaps since we only need our copy.
    DeleteObject(iinfo.hbmColor);
    DeleteObject(iinfo.hbmMask);
  } else {
    // Black and white (xor) cursor.
    color_bitmap = false;
    hbitmap.Set(iinfo.hbmMask);
  }

  if (!GetObject(hbitmap.Get(), sizeof(BITMAP), &bitmap)) {
    VLOG(3) << "Unable to get cursor bitmap. Error = " << GetLastError();
    return;
  }

  int width = bitmap.bmWidth;
  int height = bitmap.bmHeight;
  // For non-color cursors, the mask contains both an AND and an XOR mask and
  // the height includes both. Thus, the width is correct, but we need to
  // divide by 2 to get the correct mask height.
  if (!color_bitmap) {
    height /= 2;
  }
  int data_size = height * width * kBytesPerPixel;

  scoped_ptr<protocol::CursorShapeInfo> cursor_proto(
      new protocol::CursorShapeInfo());
  cursor_proto->mutable_data()->resize(data_size);
  uint8* cursor_dst_data = const_cast<uint8*>(reinterpret_cast<const uint8*>(
      cursor_proto->mutable_data()->data()));

  // Copy/convert cursor bitmap into format needed by chromotocol.
  int row_bytes = bitmap.bmWidthBytes;
  if (color_bitmap) {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 32) {
      VLOG(3) << "Unsupported color cursor format. Error = " << GetLastError();
      return;
    }

    // Cursor bitmap is stored upside-down on Windows. Flip the rows and store
    // it in the proto.
    uint8* cursor_src_data = reinterpret_cast<uint8*>(bitmap.bmBits);
    uint8* src = cursor_src_data + ((height - 1) * row_bytes);
    uint8* dst = cursor_dst_data;
    for (int row = 0; row < height; row++) {
      memcpy(dst, src, row_bytes);
      dst += width * kBytesPerPixel;
      src -= row_bytes;
    }
  } else {
    if (bitmap.bmPlanes != 1 || bitmap.bmBitsPixel != 1) {
      VLOG(3) << "Unsupported cursor mask format. Error = " << GetLastError();
      return;
    }

    // x2 because there are 2 masks in the bitmap: AND and XOR.
    int mask_bytes = height * row_bytes * 2;
    scoped_array<uint8> mask(new uint8[mask_bytes]);
    if (!GetBitmapBits(hbitmap.Get(), mask_bytes, mask.get())) {
      VLOG(3) << "Unable to get cursor mask bits. Error = " << GetLastError();
      return;
    }
    uint8* and_mask = mask.get();
    uint8* xor_mask = mask.get() + height * row_bytes;
    uint8* dst = cursor_dst_data;
    bool add_outline = false;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int byte = y * row_bytes + x / 8;
        int bit = 7 - x % 8;
        int and = and_mask[byte] & (1 << bit);
        int xor = xor_mask[byte] & (1 << bit);

        // The two cursor masks combine as follows:
        //  AND  XOR   Windows Result  Our result   RGB  Alpha
        //   0    0    Black           Black         00    ff
        //   0    1    White           White         ff    ff
        //   1    0    Screen          Transparent   00    00
        //   1    1    Reverse-screen  Black         00    ff
        // Since we don't support XOR cursors, we replace the "Reverse Screen"
        // with black. In this case, we also add an outline around the cursor
        // so that it is visible against a dark background.
        int rgb = (!and && xor) ? 0xff : 0x00;
        int alpha = (and && !xor) ? 0x00 : 0xff;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = rgb;
        *dst++ = alpha;
        if (and && xor) {
          add_outline = true;
        }
      }
    }
    if (add_outline) {
      AddCursorOutline(width, height,
                       reinterpret_cast<uint32*>(cursor_dst_data));
    }
  }

  // Compare the current cursor with the last one we sent to the client. If
  // they're the same, then don't bother sending the cursor again.
  if (last_cursor_size_.equals(width, height) &&
      memcmp(last_cursor_.get(), cursor_dst_data, data_size) == 0) {
    return;
  }

  VLOG(3) << "Sending updated cursor: " << width << "x" << height;

  cursor_proto->set_width(width);
  cursor_proto->set_height(height);
  cursor_proto->set_hotspot_x(hotspot_x);
  cursor_proto->set_hotspot_y(hotspot_y);
  cursor_shape_changed_callback_.Run(cursor_proto.Pass());

  // Record the last cursor image that we sent to the client.
  last_cursor_.reset(new uint8[data_size]);
  memcpy(last_cursor_.get(), cursor_dst_data, data_size);
  last_cursor_size_ = SkISize::Make(width, height);
}

SkISize CapturerGdi::GetScreenSize() {
  return SkISize::Make(GetSystemMetrics(SM_CXSCREEN),
                       GetSystemMetrics(SM_CYSCREEN));
}

}  // namespace

// static
Capturer* Capturer::Create() {
  return new CapturerGdi();
}

}  // namespace remoting
