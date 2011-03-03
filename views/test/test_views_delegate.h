// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/clipboard/clipboard.h"
#include "views/views_delegate.h"

namespace views {
class Window;
}

class TestViewsDelegate : public views::ViewsDelegate {
 public:
  TestViewsDelegate() {}
  virtual ~TestViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const OVERRIDE {
    if (!clipboard_.get()) {
      // Note that we need a MessageLoop for the next call to work.
      clipboard_.reset(new ui::Clipboard);
    }
    return clipboard_.get();
  }
  virtual void SaveWindowPlacement(views::Window* window,
                                   const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized) OVERRIDE {
  }
  virtual bool GetSavedWindowBounds(views::Window* window,
                                    const std::wstring& window_name,
                                    gfx::Rect* bounds) const OVERRIDE {
    return false;
  }
  virtual bool GetSavedMaximizedState(views::Window* window,
                                      const std::wstring& window_name,
                                      bool* maximized) const OVERRIDE {
    return false;
  }
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) OVERRIDE {}

#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const OVERRIDE {
    return NULL;
  }
#endif

  virtual void AddRef() OVERRIDE {}
  virtual void ReleaseRef() OVERRIDE {}

 private:
  mutable scoped_ptr<ui::Clipboard> clipboard_;

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegate);
};
