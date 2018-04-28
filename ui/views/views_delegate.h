// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_DELEGATE_H_
#define UI_VIEWS_VIEWS_DELEGATE_H_

#include <memory>
#include <string>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class ImageSkia;
class Rect;
}

namespace ui {
class ContextFactory;
}

namespace views {

class NativeWidget;
class NonClientFrameView;
class ViewsTouchEditingControllerFactory;
class View;
class Widget;

#if defined(USE_AURA)
class DesktopNativeWidgetAura;
class DesktopWindowTreeHost;
class TouchSelectionMenuRunnerViews;
#endif

namespace internal {
class NativeWidgetDelegate;
}

// ViewsDelegate is an interface implemented by an object using the views
// framework. It is used to obtain various high level application utilities
// and perform some actions such as window placement saving.
//
// The embedding app must set the ViewsDelegate instance by instantiating an
// implementation of ViewsDelegate (the constructor will set the instance).
class VIEWS_EXPORT ViewsDelegate {
 public:
  using NativeWidgetFactory =
      base::Callback<NativeWidget*(const Widget::InitParams&,
                                   internal::NativeWidgetDelegate*)>;
#if defined(USE_AURA)
  using DesktopWindowTreeHostFactory =
      base::Callback<std::unique_ptr<DesktopWindowTreeHost>(
          const Widget::InitParams&,
          internal::NativeWidgetDelegate*,
          DesktopNativeWidgetAura*)>;
#endif

#if defined(OS_WIN)
  enum AppbarAutohideEdge {
    EDGE_TOP    = 1 << 0,
    EDGE_LEFT   = 1 << 1,
    EDGE_BOTTOM = 1 << 2,
    EDGE_RIGHT  = 1 << 3,
  };
#endif

  enum class ProcessMenuAcceleratorResult {
    // The accelerator was handled while the menu was showing. No further action
    // is needed and the menu should be kept open.
    LEAVE_MENU_OPEN,

    // The accelerator was not handled. Menu should be closed and the
    // accelerator will be reposted to be handled after the menu closes.
    CLOSE_MENU
  };

  virtual ~ViewsDelegate();

  // Returns the ViewsDelegate instance if there is one, or nullptr otherwise.
  static ViewsDelegate* GetInstance();

  // Call this method to set a factory callback that will be used to construct
  // NativeWidget implementations overriding the platform defaults.
  void set_native_widget_factory(const NativeWidgetFactory& factory) {
    native_widget_factory_ = factory;
  }
  const NativeWidgetFactory& native_widget_factory() const {
    return native_widget_factory_;
  }

#if defined(USE_AURA)
  void set_desktop_window_tree_host_factory(
      const DesktopWindowTreeHostFactory& factory) {
    desktop_window_tree_host_factory_ = factory;
  }
  const DesktopWindowTreeHostFactory& desktop_window_tree_host_factory() const {
    return desktop_window_tree_host_factory_;
  }
#endif
  // Saves the position, size and "show" state for the window with the
  // specified name.
  virtual void SaveWindowPlacement(const Widget* widget,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::WindowShowState show_state);

  // Retrieves the saved position and size and "show" state for the window with
  // the specified name.
  virtual bool GetSavedWindowPlacement(const Widget* widget,
                                       const std::string& window_name,
                                       gfx::Rect* bounds,
                                       ui::WindowShowState* show_state) const;

  virtual void NotifyAccessibilityEvent(View* view,
                                        ax::mojom::Event event_type);

  // For accessibility, notify the delegate that a menu item was focused
  // so that alternate feedback (speech / magnified text) can be provided.
  virtual void NotifyMenuItemFocused(const base::string16& menu_name,
                                     const base::string16& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu);

  // If |accelerator| can be processed while a menu is showing, it will be
  // processed now and LEAVE_MENU_OPEN is returned. Otherwise, |accelerator|
  // will be reposted for processing later after the menu closes and CLOSE_MENU
  // will be returned.
  virtual ProcessMenuAcceleratorResult ProcessAcceleratorWhileMenuShowing(
      const ui::Accelerator& accelerator);

#if defined(OS_WIN)
  // Retrieves the default window icon to use for windows if none is specified.
  virtual HICON GetDefaultWindowIcon() const;
  // Retrieves the small window icon to use for windows if none is specified.
  virtual HICON GetSmallWindowIcon() const = 0;
  // Returns true if the window passed in is in the Windows 8 metro
  // environment.
  virtual bool IsWindowInMetro(gfx::NativeWindow window) const;
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  virtual gfx::ImageSkia* GetDefaultWindowIcon() const;
#endif

  // Creates a default NonClientFrameView to be used for windows that don't
  // specify their own. If this function returns NULL, the
  // views::CustomFrameView type will be used.
  virtual NonClientFrameView* CreateDefaultNonClientFrameView(Widget* widget);

  // AddRef/ReleaseRef are invoked while a menu is visible. They are used to
  // ensure we don't attempt to exit while a menu is showing.
  virtual void AddRef();
  virtual void ReleaseRef();

  // Gives the platform a chance to modify the properties of a Widget.
  virtual void OnBeforeWidgetInit(Widget::InitParams* params,
                                  internal::NativeWidgetDelegate* delegate) = 0;

  // Returns the password reveal duration for Textfield.
  virtual base::TimeDelta GetTextfieldPasswordRevealDuration();

  // Returns true if the operating system's window manager will always provide a
  // title bar with caption buttons (ignoring the setting to
  // |remove_standard_frame| in InitParams). If |maximized|, this applies to
  // maximized windows; otherwise to restored windows.
  virtual bool WindowManagerProvidesTitleBar(bool maximized);

  // Returns the context factory for new windows.
  virtual ui::ContextFactory* GetContextFactory();

  // Returns the privileged context factory for new windows.
  virtual ui::ContextFactoryPrivate* GetContextFactoryPrivate();

  // Returns the user-visible name of the application.
  virtual std::string GetApplicationName();

#if defined(OS_WIN)
  // Starts a query for the appbar autohide edges of the specified monitor and
  // returns the current value.  If the query finds the edges have changed from
  // the current value, |callback| is subsequently invoked.  If the edges have
  // not changed, |callback| is never run.
  //
  // The return value is a bitmask of AppbarAutohideEdge.
  virtual int GetAppbarAutohideEdges(HMONITOR monitor,
                                     const base::Closure& callback);
#endif

  // Whether to mirror the arrow of bubble dialogs in RTL, such that the bubble
  // opens in the opposite direction.
  virtual bool ShouldMirrorArrowsInRTL() const;

 protected:
  ViewsDelegate();

 private:
  std::unique_ptr<ViewsTouchEditingControllerFactory>
      editing_controller_factory_;

#if defined(USE_AURA)
  std::unique_ptr<TouchSelectionMenuRunnerViews> touch_selection_menu_runner_;

  DesktopWindowTreeHostFactory desktop_window_tree_host_factory_;
#endif

  NativeWidgetFactory native_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewsDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_VIEWS_DELEGATE_H_
