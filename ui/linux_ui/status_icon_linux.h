// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_UI_STATUS_ICON_LINUX_H_
#define UI_LINUX_UI_STATUS_ICON_LINUX_H_

#include "base/strings/string16.h"
#include "ui/linux_ui/linux_ui_export.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class MenuModel;
}  // namespace ui

// Since liblinux_ui cannot have dependencies on any chrome browser components
// we cannot inherit from StatusIcon. So we implement the necessary methods
// and let a wrapper class implement the StatusIcon interface and defer the
// callbacks to a delegate.
class LINUX_UI_EXPORT StatusIconLinux {
 public:
  class Delegate {
   public:
    virtual void OnClick() = 0;
    virtual bool HasClickAction() = 0;

   protected:
    virtual ~Delegate();
  };

  StatusIconLinux();
  virtual ~StatusIconLinux();

  virtual void SetImage(const gfx::ImageSkia& image) = 0;
  virtual void SetPressedImage(const gfx::ImageSkia& image) = 0;
  virtual void SetToolTip(const string16& tool_tip) = 0;

  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. The
  // subclass should destroy the existing native context menu on this call.
  virtual void UpdatePlatformContextMenu(ui::MenuModel* model) = 0;

  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  Delegate* delegate_;
};

#endif  // UI_LINUX_UI_STATUS_ICON_LINUX_H_
