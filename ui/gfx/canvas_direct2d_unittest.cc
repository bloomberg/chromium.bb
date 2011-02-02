// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include "base/command_line.h"
#include "base/ref_counted_memory.h"
#include "base/resource_util.h"
#include "base/scoped_ptr.h"
#include "gfx/brush.h"
#include "gfx/canvas_direct2d.h"
#include "gfx/canvas_skia.h"
#include "gfx/codec/png_codec.h"
#include "gfx/native_theme_win.h"
#include "gfx/rect.h"
#include "gfx/win_util.h"
#include "grit/gfx_resources.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace {

const char kVisibleModeFlag[] = "d2d-canvas-visible";
const wchar_t kWindowClassName[] = L"GFXD2DTestWindowClass";

class TestWindow {
 public:
  static const int kWindowSize = 500;
  static const int kWindowPosition = 10;

  TestWindow() : hwnd_(NULL) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      Sleep(1000);

    RegisterMyClass();

    hwnd_ = CreateWindowEx(0, kWindowClassName, NULL,
                           WS_OVERLAPPEDWINDOW,
                           kWindowPosition, kWindowPosition,
                           kWindowSize, kWindowSize,
                           NULL, NULL, NULL, this);
    DCHECK(hwnd_);

    // Initialize the RenderTarget for the window.
    rt_ = MakeHWNDRenderTarget();

    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      ShowWindow(hwnd(), SW_SHOW);
  }
  virtual ~TestWindow() {
    if (CommandLine::ForCurrentProcess()->HasSwitch(kVisibleModeFlag))
      Sleep(1000);
    DestroyWindow(hwnd());
    UnregisterMyClass();
  }

  HWND hwnd() const { return hwnd_; }

  ID2D1RenderTarget* rt() const { return rt_.get(); }

 private:
  ID2D1RenderTarget* MakeHWNDRenderTarget() {
    D2D1_RENDER_TARGET_PROPERTIES rt_properties =
        D2D1::RenderTargetProperties();
    rt_properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    ID2D1HwndRenderTarget* rt = NULL;
    gfx::CanvasDirect2D::GetD2D1Factory()->CreateHwndRenderTarget(
        rt_properties,
        D2D1::HwndRenderTargetProperties(hwnd(), D2D1::SizeU(500, 500)),
        &rt);
    return rt;
  }

  void RegisterMyClass() {
    WNDCLASSEX class_ex;
    class_ex.cbSize = sizeof(WNDCLASSEX);
    class_ex.style = CS_DBLCLKS;
    class_ex.lpfnWndProc = &DefWindowProc;
    class_ex.cbClsExtra = 0;
    class_ex.cbWndExtra = 0;
    class_ex.hInstance = NULL;
    class_ex.hIcon = NULL;
    class_ex.hCursor = LoadCursor(NULL, IDC_ARROW);
    class_ex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
    class_ex.lpszMenuName = NULL;
    class_ex.lpszClassName = kWindowClassName;
    class_ex.hIconSm = class_ex.hIcon;
    ATOM atom = RegisterClassEx(&class_ex);
    DCHECK(atom);
  }

  void UnregisterMyClass() {
    ::UnregisterClass(kWindowClassName, NULL);
  }

  HWND hwnd_;

  ScopedComPtr<ID2D1RenderTarget> rt_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

// Loads a png data blob from the data resources associated with this
// executable, decodes it and returns a SkBitmap.
SkBitmap LoadBitmapFromResources(int resource_id) {
  SkBitmap bitmap;

  HINSTANCE resource_instance = GetModuleHandle(NULL);
  void* data_ptr;
  size_t data_size;
  if (base::GetDataResourceFromModule(resource_instance, resource_id, &data_ptr,
                                      &data_size)) {
    scoped_refptr<RefCountedMemory> memory(new RefCountedStaticMemory(
        reinterpret_cast<const unsigned char*>(data_ptr), data_size));
    if (!memory)
      return bitmap;

    if (!gfx::PNGCodec::Decode(memory->front(), memory->size(), &bitmap))
      NOTREACHED() << "Unable to decode theme image resource " << resource_id;
  }
  return bitmap;
}

bool CheckForD2DCompatibility() {
  if (!gfx::Direct2dIsAvailable()) {
    LOG(WARNING) << "Test is disabled as it requires either Windows 7 or " <<
                    "Vista with Platform Update KB971644";
    return false;
  }
  return true;
}

}  // namespace

TEST(CanvasDirect2D, CreateCanvas) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());
}

TEST(CanvasDirect2D, SaveRestoreNesting) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  // Simple.
  canvas.Save();
  canvas.Restore();

  // Nested.
  canvas.Save();
  canvas.Save();
  canvas.Restore();
  canvas.Restore();

  // Simple alpha.
  canvas.SaveLayerAlpha(127);
  canvas.Restore();

  // Alpha with sub-rect.
  canvas.SaveLayerAlpha(127, gfx::Rect(20, 20, 100, 100));
  canvas.Restore();

  // Nested alpha.
  canvas.Save();
  canvas.SaveLayerAlpha(127);
  canvas.Save();
  canvas.Restore();
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, SaveLayerAlpha) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.FillRectInt(SK_ColorBLUE, 20, 20, 100, 100);
  canvas.SaveLayerAlpha(127);
  canvas.FillRectInt(SK_ColorRED, 60, 60, 100, 100);
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, SaveLayerAlphaWithBounds) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.FillRectInt(SK_ColorBLUE, 20, 20, 100, 100);
  canvas.SaveLayerAlpha(127, gfx::Rect(60, 60, 50, 50));
  canvas.FillRectInt(SK_ColorRED, 60, 60, 100, 100);
  canvas.Restore();
  canvas.Restore();
}

TEST(CanvasDirect2D, FillRect) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.FillRectInt(SK_ColorRED, 20, 20, 100, 100);
}

TEST(CanvasDirect2D, PlatformPainting) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  gfx::NativeDrawingContext dc = canvas.BeginPlatformPaint();

  // Use the system theme engine to draw a native button. This only works on a
  // GDI device context.
  RECT r = { 20, 20, 220, 80 };
  gfx::NativeTheme::instance()->PaintButton(
      dc, BP_PUSHBUTTON, PBS_NORMAL, DFCS_BUTTONPUSH, &r);

  canvas.EndPlatformPaint();
}

TEST(CanvasDirect2D, ClipRect) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.FillRectInt(SK_ColorGREEN, 0, 0, 500, 500);
  canvas.ClipRectInt(20, 20, 120, 120);
  canvas.FillRectInt(SK_ColorBLUE, 0, 0, 500, 500);
}

TEST(CanvasDirect2D, ClipRectWithTranslate) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  // Repeat the same rendering as in ClipRect...
  canvas.Save();
  canvas.FillRectInt(SK_ColorGREEN, 0, 0, 500, 500);
  canvas.ClipRectInt(20, 20, 120, 120);
  canvas.FillRectInt(SK_ColorBLUE, 0, 0, 500, 500);
  canvas.Restore();

  // ... then translate, clip and fill again relative to the new origin.
  canvas.Save();
  canvas.TranslateInt(150, 150);
  canvas.ClipRectInt(10, 10, 110, 110);
  canvas.FillRectInt(SK_ColorRED, 0, 0, 500, 500);
  canvas.Restore();
}

TEST(CanvasDirect2D, ClipRectWithScale) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  // Repeat the same rendering as in ClipRect...
  canvas.Save();
  canvas.FillRectInt(SK_ColorGREEN, 0, 0, 500, 500);
  canvas.ClipRectInt(20, 20, 120, 120);
  canvas.FillRectInt(SK_ColorBLUE, 0, 0, 500, 500);
  canvas.Restore();

  // ... then translate and scale, clip and fill again relative to the new
  // origin.
  canvas.Save();
  canvas.TranslateInt(150, 150);
  canvas.ScaleInt(2, 2);
  canvas.ClipRectInt(10, 10, 110, 110);
  canvas.FillRectInt(SK_ColorRED, 0, 0, 500, 500);
  canvas.Restore();
}

TEST(CanvasDirect2D, DrawRectInt) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.DrawRectInt(SK_ColorRED, 10, 10, 200, 200);
  canvas.Restore();
}

TEST(CanvasDirect2D, DrawLineInt) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  canvas.Save();
  canvas.DrawLineInt(SK_ColorRED, 10, 10, 210, 210);
  canvas.Restore();
}

TEST(CanvasDirect2D, DrawBitmapInt) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  SkBitmap bitmap = LoadBitmapFromResources(IDR_BITMAP_BRUSH_IMAGE);

  canvas.Save();
  canvas.DrawBitmapInt(bitmap, 100, 100);
  canvas.Restore();
}

TEST(CanvasDirect2D, DrawBitmapInt2) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  SkBitmap bitmap = LoadBitmapFromResources(IDR_BITMAP_BRUSH_IMAGE);

  canvas.Save();
  canvas.DrawBitmapInt(bitmap, 5, 5, 30, 30, 10, 10, 30, 30, false);
  canvas.DrawBitmapInt(bitmap, 5, 5, 30, 30, 110, 110, 100, 100, true);
  canvas.DrawBitmapInt(bitmap, 5, 5, 30, 30, 220, 220, 100, 100, false);
  canvas.Restore();
}

TEST(CanvasDirect2D, TileImageInt) {
  if (!CheckForD2DCompatibility())
    return;
  TestWindow window;
  gfx::CanvasDirect2D canvas(window.rt());

  SkBitmap bitmap = LoadBitmapFromResources(IDR_BITMAP_BRUSH_IMAGE);

  canvas.Save();
  canvas.TileImageInt(bitmap, 10, 10, 300, 300);
  canvas.Restore();
}
