// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_LINUX_UI_H_
#define UI_VIEWS_LINUX_UI_LINUX_UI_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/shell_dialogs/linux_shell_dialog.h"
#include "ui/views/linux_ui/status_icon_linux.h"
#include "ui/views/views_export.h"

// The main entrypoint into Linux toolkit specific code. GTK code should only
// be executed behind this interface.

namespace gfx {
class Image;
}

namespace ui {
class NativeTheme;
}

namespace views {
class WindowButtonOrderObserver;

// Adapter class with targets to render like different toolkits. Set by any
// project that wants to do linux desktop native rendering.
//
// TODO(erg): We're hardcoding GTK2, when we'll need to have backends for (at
// minimum) GTK2 and GTK3. LinuxUI::instance() should actually be a very
// complex method that pokes around with dlopen against a libuigtk2.so, a
// liuigtk3.so, etc.
class VIEWS_EXPORT LinuxUI : public ui::LinuxShellDialog {
 public:
  virtual ~LinuxUI() {}

  // Sets the dynamically loaded singleton that draws the desktop native UI.
  static void SetInstance(LinuxUI* instance);

  // Returns a LinuxUI instance for the toolkit used in the user's desktop
  // environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static LinuxUI* instance();

  // Returns an themed image per theme_provider.h
  virtual bool UseNativeTheme() const = 0;
  virtual gfx::Image GetThemeImageNamed(int id) const = 0;
  virtual bool GetColor(int id, SkColor* color) const = 0;
  virtual bool HasCustomImage(int id) const = 0;

  // Returns the preferences that we pass to WebKit.
  virtual SkColor GetFocusRingColor() const = 0;
  virtual SkColor GetThumbActiveColor() const = 0;
  virtual SkColor GetThumbInactiveColor() const = 0;
  virtual SkColor GetTrackColor() const = 0;
  virtual SkColor GetActiveSelectionBgColor() const = 0;
  virtual SkColor GetActiveSelectionFgColor() const = 0;
  virtual SkColor GetInactiveSelectionBgColor() const = 0;
  virtual SkColor GetInactiveSelectionFgColor() const = 0;
  virtual double GetCursorBlinkInterval() const = 0;

  // Returns a NativeTheme that will provide system colors and draw system
  // style widgets.
  virtual ui::NativeTheme* GetNativeTheme() const = 0;

  // Returns whether we should be using the native theme provided by this
  // object by default.
  virtual bool GetDefaultUsesSystemTheme() const = 0;

  // Sets visual properties in the desktop environment related to download
  // progress, if available.
  virtual void SetDownloadCount(int count) const = 0;
  virtual void SetProgressFraction(float percentage) const = 0;

  // Checks for platform support for status icons.
  virtual bool IsStatusIconSupported() const = 0;

  // Create a native status icon.
  virtual scoped_ptr<StatusIconLinux> CreateLinuxStatusIcon(
      const gfx::ImageSkia& image,
      const string16& tool_tip) const = 0;

  // Notifies the observer about changes about how window buttons should be
  // laid out. If the order is anything other than the default min,max,close on
  // the right, will immediately send a button change event to the observer.
  virtual void AddWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;

  // Removes the observer from the LinuxUI's list.
  virtual void RemoveWindowButtonOrderObserver(
      WindowButtonOrderObserver* observer) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_LINUX_UI_H_
