// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/rubberband_windows.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {

class RubberbandWindow {
 public:
  RubberbandWindow();
  ~RubberbandWindow();

  void Init(HWND parent, const gfx::Rect& bounds);
  HWND hwnd() const;
  void set_window_style(DWORD style);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(RubberbandWindow);
};

class RubberbandWindow::Impl : public gfx::WindowImpl {
 public:
  Impl();
  ~Impl() override;

  CR_BEGIN_MSG_MAP_EX(Impl)
    CR_MSG_WM_PAINT(OnPaint)
    CR_MSG_WM_ERASEBKGND(OnEraseBkgnd)
  CR_END_MSG_MAP()

 private:
  LRESULT OnPaint(HDC);
  LRESULT OnEraseBkgnd(HDC);

  CR_MSG_MAP_CLASS_DECLARATIONS(Impl)
  DISALLOW_COPY_AND_ASSIGN(Impl);
};

static HPEN s_pen = NULL;

RubberbandWindow::Impl::Impl() {
  if (!s_pen) {
    LOGBRUSH lb = { BS_SOLID, RGB(255, 255, 255), 0 };
    DWORD style[2] = { 2, 4 };
    s_pen = ::ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE, 1, &lb, 2, style);
  }
}

RubberbandWindow::Impl::~Impl() {
  DestroyWindow(hwnd());
}

LRESULT RubberbandWindow::Impl::OnPaint(HDC /*_dc*/) {
  RECT rect;
  ::GetClientRect(hwnd(), &rect);

  PAINTSTRUCT ps;
  HDC dc = ::BeginPaint(hwnd(), &ps);

  ::FillRect(dc, &rect, (HBRUSH)::GetStockObject(BLACK_BRUSH));

  HGDIOBJ oldPen = ::SelectObject(dc, s_pen);
  ::MoveToEx(dc, rect.left, rect.top, NULL);
  if (rect.right - rect.left == 1) {
    ::LineTo(dc, rect.right-1, rect.bottom);
  } else {
    ::LineTo(dc, rect.right, rect.bottom-1);
  }
  ::SelectObject(dc, oldPen);

  ::EndPaint(hwnd(), &ps);

  return S_OK;
}

LRESULT RubberbandWindow::Impl::OnEraseBkgnd(HDC) {
  return 1;
}

RubberbandWindow::RubberbandWindow()
: impl_(std::make_unique<Impl>()) {
}

RubberbandWindow::~RubberbandWindow() = default;

void RubberbandWindow::Init(HWND parent, const gfx::Rect& bounds) {
  impl_->Init(parent, bounds);
}

HWND RubberbandWindow::hwnd() const {
  return impl_->hwnd();
}

void RubberbandWindow::set_window_style(DWORD style) {
  impl_->set_window_style(style);
}

RubberbandOutline::RubberbandOutline() {
  for(size_t i=0; i<4; ++i) {
    windows_[i] = std::make_unique<RubberbandWindow>();
  }
}

RubberbandOutline::~RubberbandOutline() = default;

static void SetRubberbandWindowRect(RubberbandWindow& window, HWND parent,
                                    int x, int y, int w, int h) {
  if (window.hwnd() == NULL) {
    window.set_window_style(WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE);
    window.Init(parent, gfx::Rect(x, y, w, h));
    ::BringWindowToTop(window.hwnd());
  }
  else {
    ::SetWindowPos(window.hwnd(),
                   NULL,
                   x, y, w, h,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    ::InvalidateRect(window.hwnd(), NULL, FALSE);
  }
}

void RubberbandOutline::SetRect(HWND parent, RECT rect) {
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;
  SetRubberbandWindowRect(*windows_[0], parent, rect.left,  rect.top,    w, 1);
  SetRubberbandWindowRect(*windows_[1], parent, rect.right, rect.top,    1, h);
  SetRubberbandWindowRect(*windows_[2], parent, rect.left,  rect.bottom, w, 1);
  SetRubberbandWindowRect(*windows_[3], parent, rect.left,  rect.top,    1, h);
}

}  // namespace ui
