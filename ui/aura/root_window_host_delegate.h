// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_DELEGATE_H_
#define UI_AURA_ROOT_WINDOW_HOST_DELEGATE_H_

namespace gfx {
class Size;
}

namespace aura {

class KeyEvent;
class MouseEvent;
class RootWindow;
class ScrollEvent;
class TouchEvent;

// A private interface used by RootWindowHost implementations to communicate
// with their owning RootWindow.
class AURA_EXPORT RootWindowHostDelegate {
 public:
  virtual bool OnHostKeyEvent(KeyEvent* event) = 0;
  virtual bool OnHostMouseEvent(MouseEvent* event) = 0;
  virtual bool OnHostScrollEvent(ScrollEvent* event) = 0;
  virtual bool OnHostTouchEvent(TouchEvent* event) = 0;

  virtual void OnHostLostCapture() = 0;

  virtual void OnHostPaint() = 0;

  virtual void OnHostResized(const gfx::Size& size) = 0;

  virtual float GetDeviceScaleFactor() = 0;

  virtual RootWindow* AsRootWindow() = 0;

 protected:
  virtual ~RootWindowHostDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_DELEGATE_H_
