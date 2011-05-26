// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_DESKTOP_DESKTOP_WINDOW_H_
#define VIEWS_DESKTOP_DESKTOP_WINDOW_H_

#include "views/view.h"
#include "views/window/window_delegate.h"

namespace views {
namespace desktop {

class DesktopWindow : public View,
                      public WindowDelegate {
 public:
   DesktopWindow();
   virtual ~DesktopWindow();

   static void CreateDesktopWindow();

 private:
  // Overridden from View:
  virtual void Layout() OVERRIDE;

  // Overridden from WindowDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual SkBitmap GetWindowAppIcon() OVERRIDE;
  virtual SkBitmap GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual View* GetContentsView() OVERRIDE;

  void CreateTestWindow(const std::wstring& title,
                        SkColor color,
                        gfx::Rect initial_bounds,
                        bool rotate);

  DISALLOW_COPY_AND_ASSIGN(DesktopWindow);
};

}  // namespace desktop
}  // namespace views

#endif  // VIEWS_DESKTOP_DESKTOP_WINDOW_H_
